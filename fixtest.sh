if test ${1-n/a} = "-p"; then
  Name="correct"
else
  rm -f ctest.dif
  Name="ctest"
fi
if test -f bin/minerworld; then :; else
  echo "Project not built"
  exit 1
fi
rm -rf $Name
mkdir -p $Name
mv bin/minerworld $Name
mv src/*.o $Name
if test "$Name" = "ctest"; then
  diff -r --brief correct ctest >ctest.dif
fi
