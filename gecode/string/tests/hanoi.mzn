include "globals.mzn";

% Max no. of moves allowed.
int: N_MOVES = 62;
% No. of tracks (excluding the head-shunt).
int: N_TRACKS = 3;
set of int: MOVES = 1..N_MOVES;
set of int: TRACKS = 1..N_TRACKS;
string: DOMAIN = "(a|b|c|d|e)*";

% Capacities of each track.
int: A_cap = 5;
int: B_cap = 5;
int: C_cap = 5;
int: X_cap = 1;
int: MAX_CAP = max([A_cap, B_cap, C_cap, X_cap]);

% Initial configuration.
string: A_ini = "abcde";
string: B_ini = "";
string: C_ini = "";
string: X_ini = "";

% Final configuration.
string: A_fin = "";
string: B_fin = "";
string: C_fin = "abcde";
string: X_fin = "";

% Array of string variables recording the transitions after each move.
array[{0} union MOVES] of var string: A;
array[{0} union MOVES] of var string: B;
array[{0} union MOVES] of var string: C;
array[{0} union MOVES] of var string: X;

% Length differences after each move.
array[MOVES] of var -MAX_CAP..MAX_CAP: DiffA = [
  str_len(A[i]) - str_len(A[i-1]) | i in MOVES
];
array[MOVES] of var -MAX_CAP..MAX_CAP: DiffB = [
  str_len(B[i]) - str_len(B[i-1]) | i in MOVES
];
array[MOVES] of var -MAX_CAP..MAX_CAP: DiffC = [
  str_len(C[i]) - str_len(C[i-1]) | i in MOVES
];
array[MOVES] of var -MAX_CAP..MAX_CAP: DiffX = [
  str_len(X[i]) - str_len(X[i-1]) | i in MOVES
];

% Each element is a move (0 = no move).
array[MOVES] of var TRACKS: Moves;

% Initial configuration.
constraint A[0] = A_ini /\ str_len(A_ini) <= A_cap;
constraint B[0] = B_ini /\ str_len(B_ini) <= B_cap;
constraint C[0] = C_ini /\ str_len(C_ini) <= C_cap;
constraint X[0] = X_ini /\ str_len(C_ini) <= X_cap;
% Intermediate moves.
constraint forall (i in MOVES) (
  % Domain and capacity constraints.
  str_reg(A[i], DOMAIN)  /\ str_reg(B[i], DOMAIN)  /\ 
  str_reg(C[i], DOMAIN)  /\ str_reg(X[i], DOMAIN)  /\
  str_match(A[i], "ba") = 0 /\  str_match(A[i], "c(a|b)") = 0 /\ str_match(A[i], "d(a|b|c)") = 0 /\ str_match(A[i], "e(a|b|c|d)") = 0 /\
  str_match(B[i], "ba") = 0 /\  str_match(B[i], "c(a|b)") = 0 /\ str_match(B[i], "d(a|b|c)") = 0 /\ str_match(B[i], "e(a|b|c|d)") = 0 /\
  str_match(C[i], "ba") = 0 /\  str_match(C[i], "c(a|b)") = 0 /\ str_match(C[i], "d(a|b|c)") = 0 /\ str_match(C[i], "e(a|b|c|d)") = 0 /\
  str_len(A[i]) <= A_cap /\ str_len(B[i]) <= B_cap /\
  str_len(C[i]) <= C_cap /\ str_len(X[i]) <= X_cap /\
  str_len(X[i]) + str_len(X[i-1]) = 1 /\
  [% No-move: once no-move, always no-move.
   forall (j in i..N_MOVES) (
     A[i-1] = A[j] /\ B[i-1] = B[j] /\ C[i-1] = C[j] /\ Moves[j] = 0 /\
     DiffA[j] = 0  /\ DiffB[j] = 0  /\ DiffC[j] = 0  /\ DiffX[j] = 0
   ),
   % A-move.
   A[i] ++ X[i] = A[i-1] ++ X[i-1] /\ B[i] = B[i-1] /\ C[i] = C[i-1] /\
   DiffA[i] in {-1,1} /\ DiffA[i] = -DiffX[i] /\ DiffB[i] = 0 /\ DiffC[i] = 0,
   % B-move.
   B[i] ++ X[i] = B[i-1] ++ X[i-1] /\ A[i] = A[i-1] /\ C[i] = C[i-1] /\
   DiffB[i] in {-1,1}  /\ DiffB[i] = -DiffX[i] /\ DiffA[i] = 0 /\ DiffC[i] = 0,
   % C-move.
   C[i] ++ X[i] = C[i-1] ++ X[i-1] /\ A[i] = A[i-1] /\ B[i] = B[i-1] /\
   DiffC[i] in {-1,1}  /\ DiffC[i] = -DiffX[i] /\ DiffA[i] = 0 /\ DiffB[i] = 0
  ][Moves[i] + 1] /\
  % Either a move (2 tracks involved) or no moves (0 tracks involed).
  count ([DiffA[i], DiffB[i], DiffC[i], DiffX[i]], 0) in {2,4}
);
% Final configuration.
constraint A[N_MOVES] = A_fin /\ str_len(A_fin) <= A_cap;
constraint B[N_MOVES] = B_fin /\ str_len(B_fin) <= B_cap;
constraint C[N_MOVES] = C_fin /\ str_len(C_fin) <= C_cap;
constraint X[N_MOVES] = X_fin /\ str_len(X_fin) <= X_cap;

% Not using the same track twice in a row: it can be done in one move.
constraint forall (i in 2..N_MOVES) ( Moves[i-1] = Moves[i] -> Moves[i] = 0 );
% Redundant constraint
constraint sum([DiffA[i] + DiffB[i] + DiffC[i] + DiffX[i] | i in MOVES]) = 0;

% Total number of moves, to be minimized (lower bound improvable...).
var MOVES: tot_moves;
constraint tot_moves = N_MOVES - count(Moves, 0);
solve minimize tot_moves;

