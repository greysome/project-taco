#ifndef UTIL_H
#define UTIL_H

int mini(int a, int b);
int maxi(int a, int b);
int ndigits(int a);

#ifdef UTIL_IMPLEMENTATION

int mini(int a, int b) {
  return a < b ? a : b;
}

int maxi(int a, int b) {
  return a < b ? b : a;
}

int ndigits(int a) {
  if (a == 0)
    return 1;
  int i = 0;
  while ((a = a/10) > 0)
    i++;
  return i;
}

#endif // UTIL_IMPLEMENTATION
#endif // UTIL_H
