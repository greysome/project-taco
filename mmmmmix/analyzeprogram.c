//// analyzeprogram() takes in a start and end address, and outputs an
//// expression for the runtime of this portion of the code, based on
//// the number of times that certain expressions execute or certain
//// jumps are taken. The implementation is based on the discussion in
//// 2.3.4.1.
////
//// To be more specific, analyzeprogram() computes a weighted sum
//// where each term corresponds to a cycle in the directed graph of
//// the execution flow, and the weights are the signed sum of the
//// instruction counts along the cycle, where the signs indicate
//// where the edges point the right way.
////
//// Of course, the program must be in such a manner that static
//// analysis is possible. Thus it won't properly handle
//// self-modifying code (in particular it will treat "JMP *"
//// instructions literally).
////
//// For example, consider Program M of 1.3.2 (in /taocp/ch1/max.mixal),
//// which lives in addresses 0-9:
////
//// MAXIMUM	STJ	EXIT
//// INIT	ENT3	0,1
////            JMP	CHANGEM
//// LOOP	CMPA	X,3
//// 	        JGE	*+3
//// CHANGEM	ENT2	0,3
//// 	        LDA	X,3
//// 	        DEC3	1
//// 	        J3P	LOOP
//// EXIT	JMP	*
//// 	        END	MAXIMUM
////
//// The command "A1-8" gives the following output:
//// E7: # TAKEN		LDA	X,3
//// E9: # TAKEN		J3P	LOOP
//// Total time = + 3*E7 + 5*E9 + 4
////
//// E7 is 1 plus the number of times A that the maximum is changed,
//// and E9 is n-1 where n is the input size. Thus the total runtime
//// is (5n+3A+2)t, which agrees with the usual analysis of Program M.

#include "analyzeprogram.h"

typedef struct {
  int s, d;
  int id;
  // Did this edge cause a cycle?
  bool iscycle;
  // For conditional JMPs, does this edge correspond to taken or
  // non-taken branch?
  // Always true for other instructions.
  bool branch;
  // If this edge causes a cycle, we have to compute the signed sum of
  // edges along the cycle.
  int weight;
} edgeentry;

//// Compute the weight of an edgeentry with iscycle=true.
//// Returns a nonzero value if the cycle cannot be traced, which
//// means the program is not connected.
int tracecycle(int cyclestart, int cycleend, int cycleid, int Start, int End, int *parent, bool *S, edgeentry *N, edgeentry **E, mmmstate *mmm) {
  byte C, F; int instrtime;
  if (cycleend != End) {
    //// Count the edge causing the cycle itself into the sum.
    C = getC(mmm->mix.mem[cycleend]);
    F = getF(mmm->mix.mem[cycleend]);
    instrtime = getinstrtime(C, F);
    E[cycleid]->weight = instrtime;
  }

  int j = cyclestart;
  do {
    //// Find the source and destination from parent link.
    int s, d, id;
    if (S[j]) { s = j; d = parent[j]; }
    else { s = parent[j]; d = j; }
    //// One of the edges from source is in the cycle.
    edgeentry *ee = N + (s<<1);

    if (ee->d != d && (ee+1)->d != d) {
      return 1;
    }
    if (ee->d == d)
      id = ee->id;
    else
      id = (ee+1)->id;

    //// Ignore the unique edge from Start and the edge End->Start;
    //// they should not factor into our runtime.
    if (s != Start && !(s == End && d == Start)) {
      C = getC(mmm->mix.mem[s]);
      F = getF(mmm->mix.mem[s]);
      instrtime = getinstrtime(C, F);
      if (S[j])
	E[cycleid]->weight += instrtime;
      else
	E[cycleid]->weight -= instrtime;
    }

    j = parent[j];
  } while (j != cycleend);
}

//// Is this line of assembly code or data (CON/ALF)?
bool isdata(char *line) {
  char *s = line;
  // Go past the label if any
  for (; *s != ' ' && *s != '\t'; s++);
  // Go past the whitespace
  for (; *s == ' ' || *s == '\t'; s++);
  // We are at the operation field
  if (s[0] == 'C' && s[1] == 'O' && s[2] == 'N') return true;
  if (s[0] == 'A' && s[1] == 'L' && s[2] == 'F') return true;
  return false;
}

void analyzeprogram(mmmstate *mmm, int startaddr, int endaddr) {
  assert(0 <= startaddr);
  assert(startaddr <= endaddr);
  assert(endaddr < 4000);

  //// Each instruction from startaddr to endaddr is a node; there are
  //// also special Start and End nodes added at the boundaries.
  int numnodes = endaddr - startaddr + 3;
  int Start = startaddr-1, End = endaddr+1;

  //// The subroutine progressively builds an oriented subtree of the
  //// directed graph of execution flow from startaddr to endaddr.
  //// parent[j]=-1 means j has no parent;
  int *parent = malloc(numnodes * sizeof(int));
  memset(parent, -1, numnodes * sizeof(int));
  // Make parent[Start] point to the first element.
  parent -= startaddr - 1;

  //// S[j] determines whether the edge goes from j -> parent[j]
  //// (true) or parent[j] -> j (false).
  bool *S = malloc(numnodes * sizeof(bool));
  // Make S[Start] point to the first element.
  S -= startaddr - 1;

  //// Every edge has outdegree 1 or 2:
  //// 2 for conditional JMPs,
  //// 1 for non-conditional JMPs and other instructions.
  //// Thus we can store edges in a size 2*numnodes array, and the
  //// node i has entries in indices 2i and 2i+1.
  edgeentry *N = malloc(numnodes*2*sizeof(edgeentry));
  memset(N, -1, numnodes*2*sizeof(edgeentry));
  // Make N[Start<<1] point to the first element.
  N -= (startaddr-1)<<1;

  //// E allows us to access edges by their id.
  //// Each entry points to an entry of N.
  edgeentry **E = malloc(numnodes*2*sizeof(edgeentry *));

  int curid = 0;
  byte C, F; int A;
  for (int i = Start; i <= End; i++) {
    if (isdata(mmm->debuglines[i])) continue;
    if (i == Start) {     // Start has exactly one edge: to startaddr
      C = JMP; F = 0; A = startaddr;
    }
    else if (i == End) {  // End has exactly one edge: to Start
      C = JMP; F = 0; A = Start;
    }
    else {
      C = getC(mmm->mix.mem[i]);
      F = getF(mmm->mix.mem[i]);
      A = INTA(getA(mmm->mix.mem[i]));
    }

    // The (up to) two destination nodes from i.
    int d1, d2;
    bool conditionaljmp = false, firstedge = true;

    // All non-JMPs are essentially JMPs to the next instruction.
    if (C != JMP && (C < JMPA || C > JMPX)) A = i+1;
    // All JMPs outside are redirected to End.
    else if (i != End && (A < startaddr || A > endaddr)) A = End;
    // All HLTs as well.
    if (C == SPEC && F == 2) A = End;
    // The first edge is always set.
    d1 = A;
    // The second edge is only set for conditional JMPs.
    if ((C == JMP && F > 0) || (C >= JMPA && C <= JMPX)) {
      conditionaljmp = true;
      d2 = i+1;
    }

nextedge:
    //// Make i the root (based on the solution to 2.3.4.2-11).
    int j = i, k = parent[j];
    parent[j] = -1;
    bool s = S[j];
    while (k != -1) {
      int k1 = parent[k];
      bool s1 = S[k];
      parent[k] = j;
      S[k] = !s;
      s = s1; j = k; k = k1;
    }

    //// Record in edge entry list.
    edgeentry *ee = N + (i<<1);
    // Is the first entry for i already populated?
    if (ee->d >= 0) ee++;
    ee->s = i;
    ee->d = d1;
    ee->id = curid;
    ee->branch = firstedge;
    ee->weight = 0;
    E[curid] = ee;

    //// Check if there is a cycle if we add the edge i->d, i.e. is
    //// there a path from i to d?
    for (j = d1; parent[j] != -1; j = parent[j]);
    if (j == i) {
      //printf("CYCLE %d->%d\n", i, d1);
      ee->iscycle = true;
      if (tracecycle(d1, i, curid, Start, End, parent, S, N, E, mmm)) {
	printf("There is no way to go from %d to %d! Please double-check your range!\n", startaddr, endaddr);
	return;
      }
    }
    else {
      //printf("ADD %d->%d\n", i, d1);
      //// No cycle, proceed to add the edge
      ee->iscycle = false;
      parent[i] = d1;
      S[i] = true;
    }

    curid++;

    if (conditionaljmp) {
      d1 = d2;
      firstedge = false;
      conditionaljmp = false;
      goto nextedge;
    }
  }

  for (int i = 0; i < curid; i++) {
    edgeentry *ee = E[i];
    if (!ee->iscycle || ee->s == End) continue;
    printf("E%d: %s\t%s\n", i, ee->branch ? "# TAKEN" : "# UNTAKEN", mmm->debuglines[ee->s]);
  }
  printf("Total time = ");
  for (int i = 0; i < curid; i++) {
    edgeentry *ee = E[i];
    if (!ee->iscycle || ee->weight == 0) continue;
    if (i == curid-1)
      printf("+ %d", ee->weight, i);
    else
      printf("+ %d*E%d ", ee->weight, i);
  }
  printf("\n");

  free(parent + startaddr-1);
  free(S + startaddr-1);
  free(N + ((startaddr-1)<<1));
  free(E);
}