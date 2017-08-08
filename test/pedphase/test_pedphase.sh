#!/usr/bin/env bash

echo "Test 1"
./akt pedphase test/pedphase/test1.vcf.gz -p test/pedphase/pedigree.fam -o - 2> /dev/null | bcftools view -H > test/pedphase/test1.out
diff test/pedphase/test1.out test/pedphase/test1.vcf
echo "PASSED"

echo "Test 2"
./akt pedphase test/pedphase/test2.vcf.gz -p test/pedphase/pedigree.fam -o - 2> /dev/null | bcftools view -H > test/pedphase/test2.out
diff test/pedphase/test2.out test/pedphase/test2.vcf
echo "PASSED"

echo "Test 3"
./akt pedphase test/pedphase/test3.vcf.gz -p test/pedphase/pedigree.fam -o - 2> /dev/null | bcftools view -H > test/pedphase/test3.out
diff test/pedphase/test3.out test/pedphase/test3.vcf
echo "PASSED"

echo "Test 5"
./akt pedphase test/pedphase/test5.vcf.gz -p test/pedphase/pedigree.fam -o - 2> /dev/null | bcftools view -H > test/pedphase/test5.out
diff test/pedphase/test5.out test/pedphase/test5.vcf
echo "PASSED"

echo "ALL TESTS PASSED!"
