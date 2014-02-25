test_opencl:
	g++ -std=c++11 -O2 src/test_opencl.cpp -I ./include/ -I ./opencl_sdk/include -L ./opencl_sdk/lib/cygwin/x86 -l OpenCL -o ./bin/test_opencl
		
make_world: 
	g++ -std=c++11 -O2 src/heat.cpp src/make_world.cpp -I ./include/ -I ./opencl_sdk/include -o ./bin/make_world
	
step_world: 
	g++ -std=c++11 -O2 src/heat.cpp src/step_world.cpp -I ./include/ -I ./opencl_sdk/include -o ./bin/step_world
	
render_world: 
	g++ -std=c++11 -O2 src/heat.cpp src/render_world.cpp -I ./include/ -I ./opencl_sdk/include -o ./bin/render_world
	
step_world_v1_lamda: 
	g++ -std=c++11 -O2 src/heat.cpp src/bcs10/step_world_v1_lamda.cpp -I ./include/ -I ./opencl_sdk/include -o ./bin/step_world_v1_lamda
	
step_world_v2_lamda: 
	g++ -std=c++11 -O2 src/heat.cpp src/bcs10/step_world_v2_lamda.cpp -I ./include/ -I ./opencl_sdk/include -o ./bin/step_world_v2_lamda
	
step_world_v3_opencl: 
	g++ -std=c++11 -O2 src/heat.cpp src/bcs10/step_world_v3_opencl.cpp -I ./include/ -I ./opencl_sdk/include -L ./opencl_sdk/lib/cygwin/x86 -l OpenCL -o ./bin/step_world_v3_opencl
	
step_world_v4_double_buffered: 
	g++ -std=c++11 -O2 src/heat.cpp src/bcs10/step_world_v4_double_buffered.cpp -I ./include/ -I ./opencl_sdk/include -L ./opencl_sdk/lib/cygwin/x86 -l OpenCL -o ./bin/step_world_v4_double_buffered
	
step_world_v5_packed_properties: 
	g++ -std=c++11 -O2 src/heat.cpp src/bcs10/step_world_v5_packed_properties.cpp -I ./include/ -I ./opencl_sdk/include -L ./opencl_sdk/lib/cygwin/x86 -l OpenCL -o ./bin/step_world_v5_packed_properties

all: step_world_v5_packed_properties step_world_v4_double_buffered step_world_v3_opencl step_world_v2_lamda step_world_v1_lamda render_world step_world make_world
