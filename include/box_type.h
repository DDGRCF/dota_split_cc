#ifndef BOX_TYPE_H_
#define BOX_TYPE_H_

struct HBox {
  int x1, y1, x2, y2;
};

struct Rbox {
  float theta;
  int x, y, w, h;
};

struct QBox {
  int x1, y1, x2, y2, x3, y3, x4, y4;
};

#endif
