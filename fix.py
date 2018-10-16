import os

def FixFile(Name):
  F=open(Name)
  NewName=Name+".out"
  OutF=open(NewName,"w")
  EmptyLineCount=0
  LineNum=0
  ReportFileName=True
  ExpectedPos=0
  while True:
    LineNum+=1
    Line=F.readline()
    if Line=="":
      break
    Len=len(Line)-1
    while Len>0:
      if Line[Len-1] not in " \t":
        break
      Len-=1
    Line=Line[:Len]
    if Line=="":
      EmptyLineCount+=1
      continue
    while EmptyLineCount>0:
      OutF.write("\n")
      EmptyLineCount-=1
    Pos=0
    NewPos=0
    for Ch in Line:
      if Ch=="\t":
        NewPos=(NewPos+4)&0xFFC
      elif Ch==" ":
        NewPos+=1
      else:
        break
      Pos+=1
    if ExpectedPos!=0:
      if NewPos==ExpectedPos:
        NewPos-=PosDelta
      else:
        ExpectedPos=0
    Segment=Line[Pos:]
    if ExpectedPos==0:
      PosDelta=0
      if Segment.startswith("NOTE:"):
        PosDelta=6
      elif Segment.startswith("- "):
        PosDelta=2
      else:
        Index=Segment.find(":")
        if Index>0:
          PosDelta=Index+2
      if PosDelta>0:
        ExpectedPos=NewPos+PosDelta
    if NewPos%4!=0:
      if ReportFileName:
        print "In file",Name
        ReportFileName=False
      print "%5d: Indentation not a multiple of 4"%LineNum
    else:
      Line=" "*(NewPos/2)+Segment
    OutF.write(Line)
    OutF.write("\n")
  OutF.close()
  F.close()
  os.unlink(Name)
  os.rename(NewName,Name)

def Main():
  List=os.listdir("src")
  for Name in List:
    if Name.endswith(".h") or Name.endswith(".cpp"):
      FixFile("src/"+Name)

Main()
