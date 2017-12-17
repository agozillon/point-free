/// To the left of the "=>" is the starting function/lambda written in Haskell, to the right is the ideal/correct PointFree 
/// output also written in haskell most of the examples in here are attempts at base cases, the least complex examples
/// that will hopefully prove that the system works as it should (at least on smaller blocks of code)

/// ---------- Simplest Examples -----------

/// Example: \x -> x => id
int ReturnX(int x) {
    return x;
}

/// Example: \x -> y => const y 
/// is this an accurate representation...?
int yy = 10;
int ReturnY(int x) {
    return yy;
}

/// is this more accurate...?
int ReturnY2(int x) {
    return ReturnX(10);
}

/// ---------- More Complex Examples -----------

/// Example: \x -> x + x => ap (+) id
int AddX(int x) {
    return x + x;
}

/// Example: \x -> x - 2 => flip (-) 2 
int Negate2(int x) {
    return x - 2;
}

/// Example: \x -> 1 + x => (+) 1  
int Increment1(int x) {
    return 1 + x; 
}

/// Example: \x y -> z + x + y => (((.) (+)) (z +))
/// Writing two again as I'm not sure which version is more suitable at the moment
int zz = 10;
int AddXYZ(int x, int y) {
    return zz + x + y;
}

int AddXYZ2(int x, int y) {
    return ReturnX(10) + x + y;
}

/// Example: \x -> 1 => const 1
int Return1(int x){
    return 1;
}

/// ---------- Templated Examples -----------

// Need to come up with some reasonable template examples next. 