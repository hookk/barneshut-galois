#include <iostream>

#include "Completeness.h"
#include <Galois/Accumulator.h>

#include "Rng.h"

/**
 * Functor
 *
 * Compute radiance for all rays
 */
struct CastRays {
	typedef int tt_does_not_need_stats;
	typedef int tt_does_not_need_aborts;

	// camera reference
	const Camera& cam;

	// object tree
	const BVHTree* tree;

	// result image
	Image& img;

	// current pixel being processed
	Pixel &pixel;

	// global config reference
	const Config& config;

	// how many rays left to process
	Galois::GAccumulator<uint>& accum;

	// cache misses accumulator
	Galois::GAccumulator<long long int>& counter_accum;

	// whats the depth of the current rays?
	const uint depth;

	// random number generators
	std::vector<RNG>& rngs;

	/**
	 * Constructor
	 */
	CastRays(const Camera& _cam,
				const BVHTree* _tree,
				Image& _img,
				Pixel& _pixel,
				const Config& _config,
				Galois::GAccumulator<uint>& _accum,
				Galois::GAccumulator<long long int>& _counter_accum,
				const uint _depth,
				std::vector<RNG>& _rngs)
	:	cam(_cam),
		tree(_tree),
		img(_img),
		pixel(_pixel),
		config(_config),
		accum(_accum),
		counter_accum(_counter_accum),
		depth(_depth),
		rngs(_rngs)
	{ }

	/**
	 * Functor
	 */
	// receive a block of pixels instead
	template<typename Context>
	void operator()(Ray** _ray, Context&) {
		Ray& ray = **_ray;

		if (ray.valid)
			radiance(ray, rngs[GaloisRuntime::LL::getTID()]);
	}


	private:

	void radiance(Ray &ray, RNG& rng){ 
		// distance to intersection 
		double dist;

		// id of intersected object 
		Object* obj_ptr;

		int papi_set = PAPI_NULL;
		long long int value;
		if (config.papi) {
#ifndef NDEBUG
			assert(PAPI_create_eventset(&papi_set) == PAPI_OK);
			assert(PAPI_add_event(papi_set, PAPI_L2_DCM) == PAPI_OK);
			assert(PAPI_start(papi_set) == PAPI_OK);
#else
			int event;
			char * name = "LLC_MISSES";
			PAPI_event_name_to_code(name, &event);
			PAPI_create_eventset(&papi_set);
			PAPI_add_event(papi_set, event);
			PAPI_start(papi_set);
#endif
		}
		bool intersected = tree->intersect(ray, dist, obj_ptr);
		if (config.papi) {
#ifndef NDEBUG
			assert(PAPI_stop(papi_set, &value) == PAPI_OK);
			counter_accum.get() += value;
			assert(PAPI_cleanup_eventset(papi_set) == PAPI_OK);
			assert(PAPI_destroy_eventset(&papi_set) == PAPI_OK);
#else
			PAPI_stop(papi_set, &value);
			counter_accum.get() += value;
			PAPI_cleanup_eventset(papi_set);
			PAPI_destroy_eventset(&papi_set);
#endif		
		}

		// if miss, return black
		if (!intersected) {
			ray.valid = false;
			accum.get() += 1;
			return;
		}

		// the hit object 
		const Sphere &obj = *static_cast<Sphere*>(obj_ptr);
		Vec f = obj.color;

		ray.weightedAdd(obj.emission);

		//Russian Roullete to stop
		if (depth > config.maxdepth) {
			double max_refl = obj.color.max_coord();
			if (rng() < max_refl)
				f *= (1 / max_refl);
			else {
				ray.valid = false;
				accum.get() += 1;
				return;
			}
		}

		Vec innerResult;

		Vec hit_point = ray.orig + ray.dir * dist;
		Vec norm = (hit_point - obj.pos).norm();
		Vec nl   = norm.dot(ray.dir) < 0 ? norm : (norm * -1);

		/** To blockalize:
		 *     each case generates a new ray except REFR which can generate 2.
		 *     Insert each generated ray into new block and recurse on it. Then add the result to each of the previous rays
		 *     Return value is common to every case, so no problem there */
		switch(obj.refl) {
			case DIFF: // Ideal DIFFUSE reflection
				computeDiffuseRay(ray, rng, hit_point, nl);
				break;

			case SPEC: // Ideal SPECULAR reflection
				computeSpecularRay(ray, hit_point, norm, ray.dir);
				break;

			case REFR: // Ideal dielectric REFRACTION
				computeRefractedRay(ray, rng, norm, nl, hit_point);
				break;

			default:
				assert(false && "Invalid object reflection type");
				abort();
		}

		ray.weight *= f;
	}

	/**
	 * Sub-ray calculation
	 */

	// creates a diffuse ray
	void computeDiffuseRay(Ray& ray, RNG& rng, Vec& hit_point, Vec& nl) const {
		double r1  = rng() * 2 * M_PI;
		double r2  = rng();
		double r2s = sqrt(r2);

		Vec w   = nl;
		Vec u   = ((fabs(w.x) > 0.1 ? Vec((double)0, 1) : Vec(1)) % w).norm();
		Vec v   = w % u;
		Vec dir = (u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)).norm(); 

		ray.orig = hit_point;
		ray.dir  = dir;
	}

	// creates a specular ray
	void computeSpecularRay(Ray& ray, const Vec& hit_point, const Vec& norm, const Vec& r_dir) const {
		ray.orig = hit_point;
		ray.dir  = r_dir - norm * 2 * norm.dot(r_dir);
	}

	// creates a reflected ray
	void computeReflectedRay(Ray& ray, const Vec& orig, const Vec& dir) const {
		ray.orig = orig;
		ray.dir  = dir;
	}

	// creates a refracted ray
	void computeRefractedRay(Ray& ray, RNG& rng, const Vec& norm, const Vec& nl, const Vec& hit_point) const {
		const double nc    = 1;
		const double nt    = 1.5;
		const bool   into  = norm.dot(nl) > 0;
		const double nnt   = into ? nc / nt : nt / nc;
		const double ddn   = ray.dir.dot(nl);
		const double cos2t = 1 - nnt * nnt * (1 - ddn * ddn);
		double weight = 1.0;

		// Total internal reflection
		if (cos2t < 0) {
			computeSpecularRay(ray, hit_point, norm, ray.dir);
		} else {
			Vec tdir = (ray.dir * nnt - norm * ((into ? 1 : -1) * (ddn * nnt + sqrt(cos2t)) )).norm();
			double a = nt - nc;
			double b = nt + nc;
			double R0 = a * a / (b * b);
			double c = 1 - (into ? -ddn : tdir.dot(norm));
			double Re = R0 + (1 - R0) * c * c * c * c * c;
			double Tr = 1 - Re; 

			//if (depth > 2)

			double P = 0.25 + 0.5 * Re;
			if (rng() < P) {
				computeSpecularRay(ray, hit_point, norm, ray.dir);
				weight = Re / P;
			}
			else {
				computeReflectedRay(ray, hit_point, tdir);
				weight = Tr / (1 - P);
			}

			//} else { // otherwise, sum both
			//	refrResult = radiance(reflRay, depth, Xi) * Re + radiance(refrRay, depth, Xi) * Tr;
			//}
		}
		
		ray.weight *= weight;
	}
};