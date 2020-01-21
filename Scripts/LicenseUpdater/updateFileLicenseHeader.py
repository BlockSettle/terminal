# updates the copyright information for all specified files recursively
# usage: python3 updateFileLicenseHeader.py
#
# * in the same folder should be two files: oldlicense.txt & newlicense.txt
# * script update only all fiels which is started from text which is could be find inside oldlicense.txt(or empty if there is not license header)
# * specify all directories you want to exclude inside excludedir array
# * specify correct comment symbols inside recursive_traversal for every new programming language 


import os

excludedir = ['ArmoryDB', 'UnitTests', 'AuthAPI']

def update_source(filename, oldcopyright, copyright):
   utfstr = chr(0xef)+chr(0xbb)+chr(0xbf)
   with open(filename, 'r') as filetochange: fdata = filetochange.read()
   isUTF = False
   if (fdata.startswith(utfstr)):
      isUTF = True
      fdata = fdata[3:]
   if (oldcopyright != None):
      if (fdata.startswith(oldcopyright)):
         fdata = fdata[len(oldcopyright):]
   if not (fdata.startswith(copyright)):
      print("updating " + filename)
      fdata = copyright + fdata
      if (isUTF):
         with open(filename, 'w') as modified: modified.write(utfstr+fdata)
      else:
         with open(filename, 'w') as modified: modified.write(fdata)

def recursive_traversal(dir, oldcopyright, copyright):
   fns = os.listdir(dir)
   for fn in fns:
      if (fn in excludedir):
        continue;
       
      fullfn = os.path.join(dir,fn)

      if (os.path.isdir(fullfn)):
         recursive_traversal(fullfn, oldcopyright, copyright)
      else:
         if (fullfn.endswith(".hpp") or fullfn.endswith(".cpp") or fullfn.endswith(".h") or fullfn.endswith(".c") or fullfn.endswith(".qml")):
            update_source(fullfn, "/*\n\n" + oldcopyright + "\n*/\n", "/*\n\n" + copyright + "\n*/\n")

with open('oldlicense.txt', 'r') as oldfile: oldlicense = oldfile.read()
with open('newlicense.txt', 'r') as newfile: newlicense = newfile.read()
recursive_traversal(".", oldlicense, newlicense)
exit()
