ORIG_CC=`which gcc-6`
ORIG_CXX=`which g++-6`
HERE=`pwd`
CHECKER="$HERE/util/travis/coding-standards-checker.py"
export CC="$HERE/util/travis/hooks/gcc $CHECKER $ORIG_CC"
export CXX="$HERE/util/travis/hooks/g++ $CHECKER $ORIG_CXX"
