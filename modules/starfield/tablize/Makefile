all : tablize flat_stars.dat

#hip_main.dat :
#	wget http://cdsarc.u-strasbg.fr/ftp/cats/I/239/hip_main.dat.gz
#	gunzip hip_main.dat.gz

constellationship.fab :
	wget https://raw.githubusercontent.com/Stellarium/stellarium/master/skycultures/western_SnT/constellationship.fab

hip2.dat :
	wget http://cdsarc.u-strasbg.fr/ftp/I/311/hip2.dat.gz
	gunzip hip2.dat.gz

flat_stars.dat : tablize hip2.dat constellationship.fab
	./tablize

tablize : tablize.c
	gcc -o $@ tablize.c -O1 -g -I../../../cntools/cnrbtree

clean :
	rm -rf tablize

