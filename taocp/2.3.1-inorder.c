// 2.3.1-31,32: Traverse an unthreaded binary tree in inorder without
// using a stack
#include <stdio.h>

struct _node {
  int val;
  struct _node *l;
  struct _node *r;
};
typedef struct _node node;

void visit(node *v) {
  printf("%d\n", v->val);
}

void inorder(node *root) {
  node *P = root, *R = NULL, *Q;
  while (P != NULL) { // Are there still nodes to visit?
    Q = P->l;
    if (Q != NULL) {  // Is there a left subtree to recurse on?
      for (; !(Q == R || Q->r == NULL); Q = Q->r);  // Go all the way to the right.
      if (Q != R) { Q->r = P; P = P->l; continue; } // If there is no thread, create the thread and recurse on left subtree.
      Q->r = NULL;    // Otherwise (i.e. we arrived at P via a thread), remove it.
    }
    visit(P);         // We arrive here if there is no left subtree or it is already visited.
    R = P; P = P->r;  // Advance P. (P->r may be a thread in which case R=$P.)
  }
}

int main() {
  // The tree in 2.3.1-(7)
  node A, B, C, D, E, F, G, H, J;
  A.val = 1; A.l =   &B; A.r =   &C;
  B.val = 2; B.l =   &D; B.r = NULL;
  C.val = 3; C.l =   &E; C.r =   &F;
  D.val = 4; D.l = NULL; D.r = NULL;
  E.val = 5; E.l = NULL; E.r =   &G;
  F.val = 6; F.l =   &H; F.r =   &J;
  G.val = 7; G.l = NULL; G.r = NULL;
  H.val = 8; H.l = NULL; H.r = NULL;
  J.val = 9; J.l = NULL; J.r = NULL;
  inorder(&A);
  return 0;
}