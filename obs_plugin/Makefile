all : shm_texturedata_obs.so

shm_texturedata_obs.so : shm_texturedata_obs.c
	gcc -o $@ $^ -lobs -shared -fPIC -lGL -O3
	mkdir -p ~/.config/obs-studio/plugins/shm_texturedata_obs/bin/64bit
	cp shm_texturedata_obs.so ~/.config/obs-studio/plugins/shm_texturedata_obs/bin/64bit/

remove :
	rm ~/.config/obs-studio/plugins/shm_texturedata_obs/bin/64bit/shm_texturedata_obs.so

clean :
	rm -rf shm_texturedata_obs.so


