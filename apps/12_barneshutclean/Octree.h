/** Abstract class for an Octree node.
 * A node in an octree is either an internal node or a body (leaf).
 */
struct Octree {
  double mass;
  Point pos;
  virtual ~Octree() { }
  virtual bool isLeaf() const = 0;

protected:
  Octree() : mass(0.0) { }
  Octree(double _mass) : mass(_mass) { }
};



/** Internal node in an Octree.
 * These nodes have pointers for at most 8 other nodes. They also have position and mass.
 */
struct OctreeInternal : Octree {
  Octree* child[8];
  Point pos;
  OctreeInternal(Point _pos) : pos(_pos) {
    bzero(child, sizeof(*child) * 8);
  }
  virtual ~OctreeInternal() {
    for (int i = 0; i < 8; i++) {
      if (child[i] != NULL && !child[i]->isLeaf()) {
        delete child[i];
      }
    }
  }
  virtual bool isLeaf() const {
    return false;
  }
};


/** Leaf node in an Octree. Represents the bodies in the n-body problem.
 * These nodes have position, velocity, acceleration and mass.
 */
struct Body : Octree {
  int id;
  Point vel;
  Point acc;
  Body() { }
  virtual bool isLeaf() const {
    return true;
  }
};

//  Output operator for the leaf octree nodes.
std::ostream& operator<<(std::ostream& os, const Body& b) {
  os << "(pos:" << b.pos
     << " vel:" << b.vel
     << " acc:" << b.acc
     << " mass:" << b.mass << ")";
  return os;
}