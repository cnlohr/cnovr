all : shm_opengl_obs.so

shm_opengl_obs.so : shm_opengl_obs.c
	gcc -o $@ $^ -lobs -shared -fPIC -lGL
	mkdir -p ~/.config/obs-studio/plugins/shm_opengl_obs/bin/64bit
	cp shm_opengl_obs.so ~/.config/obs-studio/plugins/shm_opengl_obs/bin/64bit/
remove :
	rm ~/.config/obs-studio/plugins/shm_opengl_obs/bin/64bit/shm_opengl_obs.so

clean :
	rm -rf shm_opengl_obs.so


