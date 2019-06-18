#!/bin/sh

# This file is part of varbit.
# varbit is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# varbit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with varbit.  If not, see <http://www.gnu.org/licenses/>.

if [ -z $1 ]; then
   echo "Usage: $0 <db_path>"
   echo
   echo "<db_path> is the path to a database created with varbit."
   exit 1
fi

DB_LOC=$1

# Keep trying until the database is unlocked.
false
until [ $? -eq 0 ]; do
   HASHES=`echo "select hash_contents from files group by hash_contents having count(*) > 1;" | sqlite3 $DB_LOC`
done

SIZE_TOTAL=0

for HASH_ITER in $HASHES; do
   # Keep trying until the database is unlocked.
   false
   until [ $? -eq 0 ]; do
      DUPES=`echo "select path from files where hash_contents=$HASH_ITER;" | \
         sqlite3 $DB_LOC 2>/dev/null`
   done

   # Verify dupes are actually dupes.
   OLDIFS=$IFS
   IFS=$'\n'
   OLD_SHA1="x"
   FALSE_DUPE=0
   DUPES_OUT=""
   SIZE_DUPE=0
   for DUPE_ITER in $DUPES; do
      SHA1_ITER=`sha1sum -b "$DUPE_ITER" | awk '{print $1}'`
      SIZE_ITER=`du -b "$DUPE_ITER" | awk '{print $1}'`
      if [ "$OLD_SHA1" != "x" ] && [ "$OLD_SHA1" != "$SHA1_ITER" ]; then
         echo FALSE
         FALSE_DUPE=1
      elif [ "$OLD_SHA1" != "x" ] && [ "$OLD_SHA1" = "$SHA1_ITER" ]; then
         SIZE_TOTAL=$(($SIZE_TOTAL + $SIZE_ITER))
         SIZE_DUPE=$((SIZE_DUPE + $SIZE_ITER))
      elif [ "$OLD_SHA1" = "x" ]; then
         OLD_SHA1=$SHA1_ITER
      fi

      DUPES_OUT="$DUPES_OUT\n$SHA1_ITER $DUPE_ITER"
   done
   IFS=$OLDIFS

   # Give a final report for this set of hashes.
   if [ 0 -eq $FALSE_DUPE ]; then
      echo
      echo "== HASH $HASH_ITER ($SIZE_DUPE bytes) =="
      echo -e "$DUPES_OUT"
   else
      echo "== HASH $HASH_ITER =="
      echo
      echo "*** False dupe detected! ***"
      echo -e "$DUPES_OUT"
   fi
done

echo $SIZE_TOTAL bytes in dupes.

