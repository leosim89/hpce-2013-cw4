#include "heat.hpp"

#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <memory>
#include <cstdio>
#include <string>
#include <cstdlib>

#include <fstream>
#include <streambuf>

# include <alloca.h>
#define __CL_ENABLE_EXCEPTIONS 
#include "CL/cl.hpp"

namespace hpce{

namespace bcs10{

std::string LoadSource(const char *fileName)
	{
		// Don't forget to change your_login here
		std::string baseDir="src/bcs10";
		if(getenv("HPCE_CL_SRC_DIR")){
			baseDir=getenv("HPCE_CL_SRC_DIR");
		}
		
		std::string fullName=baseDir+"/"+fileName;
		
		std::ifstream src(fullName, std::ios::in | std::ios::binary);
		if(!src.is_open())
			throw std::runtime_error("LoadSource : Couldn't load cl file from '"+fullName+"'.");
		
		return std::string(
			(std::istreambuf_iterator<char>(src)), // Node the extra brackets.
            std::istreambuf_iterator<char>()
		);
	}

void StepWorldV5PackedProperties(world_t &world, float dt, unsigned n)
{
	std::vector<cl::Platform> platforms;
	
	cl::Platform::get(&platforms);
	if(platforms.size()==0)
		throw std::runtime_error("No OpenCL platforms found.");
	
	std::cerr<<"Found "<<platforms.size()<<" platforms\n";
	for(unsigned i=0;i<platforms.size();i++){
		std::string vendor=platforms[i].getInfo<CL_PLATFORM_VENDOR>();
		std::cerr<<"  Platform "<<i<<" : "<<vendor<<"\n";
	}
	
	int selectedPlatform=0;
	if(getenv("HPCE_SELECT_PLATFORM")){
		selectedPlatform=atoi(getenv("HPCE_SELECT_PLATFORM"));
	}
	std::cerr<<"Choosing platform "<<selectedPlatform<<"\n";
	cl::Platform platform=platforms.at(selectedPlatform);    
	
	std::vector<cl::Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);	
	if(devices.size()==0){
		throw std::runtime_error("No opencl devices found.\n");
	}
		
	std::cerr<<"Found "<<devices.size()<<" devices\n";
	for(unsigned i=0;i<devices.size();i++){
		std::string name=devices[i].getInfo<CL_DEVICE_NAME>();
		std::cerr<<"  Device "<<i<<" : "<<name<<"\n";
	}
	
	int selectedDevice=0;
	if(getenv("HPCE_SELECT_DEVICE")){
		selectedDevice=atoi(getenv("HPCE_SELECT_DEVICE"));
	}
	std::cerr<<"Choosing device "<<selectedDevice<<"\n";
	cl::Device device=devices.at(selectedDevice);
	
	cl::Context context(devices);
	
	std::string kernelSource=LoadSource("step_world_v5_kernel.cl");
	
	cl::Program::Sources sources;	// A vector of (data,length) pairs
	sources.push_back(std::make_pair(kernelSource.c_str(), kernelSource.size()+1));	// push on our single string
	
	cl::Program program(context, sources);
	try{
		program.build(devices);
	}catch(...){
		for(unsigned i=0;i<devices.size();i++){
			std::cerr<<"Log for device "<<devices[i].getInfo<CL_DEVICE_NAME>()<<":\n\n";
			std::cerr<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[i])<<"\n\n";
		}
		throw;
	}
	
	size_t cbBuffer=4*world.w*world.h;
	cl::Buffer buffProperties(context, CL_MEM_READ_ONLY, cbBuffer);
	cl::Buffer buffState(context, CL_MEM_READ_WRITE, cbBuffer);
	cl::Buffer buffBuffer(context, CL_MEM_READ_WRITE, cbBuffer);
	
	float outer=world.alpha*dt;		// We spread alpha to other cells per time
	float inner=1-outer/4;				// Anything that doesn't spread stays
	unsigned w=world.w, h=world.h;
	// This is our temporary working space
	std::vector<float> buffer(w*h);
	
	// Generating the packed properties
	// 1 = Normal. 0 = Insulator
	// Bit 3: Above (y-)
	// Bit 4: Below (y+)
	// Bit 5: Left (x-)
	// Bit 6: Right (x+)
	
	std::vector<uint32_t> packed(w*h, 0);
	packed.assign(world.properties.begin(), world.properties.end());
	for(unsigned y=0;y<h;y++){
		for(unsigned x=0;x<w;x++){
			unsigned index=y*w + x;
			if (!packed[index]) {
				if (!(packed[index-w] & Cell_Insulator))
					packed[index] = packed[index] + 0x04;
				if (!(packed[index+w] & Cell_Insulator))
					packed[index] = packed[index] + 0x08;
				if (!(packed[index-1] & Cell_Insulator))
					packed[index] = packed[index] + 0x10;
				if (!(packed[index+1] & Cell_Insulator))
					packed[index] = packed[index] + 0x20;
			}
		}
	}
	
	cl::Kernel kernel(program, "kernel_xy");
	
	kernel.setArg(0, inner);
	kernel.setArg(1, outer);
	kernel.setArg(2, buffProperties);
	
	cl::CommandQueue queue(context, device);
	
	queue.enqueueWriteBuffer(buffProperties, CL_TRUE, 0, cbBuffer, &packed[0]);

	cl::NDRange offset(0, 0);				// Always start iterations at x=0, y=0
	cl::NDRange globalSize(w, h);	// Global size must match the original loops
	cl::NDRange localSize=cl::NullRange;	// We don't care about local size
	
	queue.enqueueWriteBuffer(buffState, CL_TRUE, 0, cbBuffer, &world.state[0]);	
	
	for(unsigned t=0;t<n;t++){
		kernel.setArg(3, buffState);
		kernel.setArg(4, buffBuffer);
		queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize);
		queue.enqueueBarrier();	
		
		std::swap(buffState, buffBuffer);
	
		world.t += dt; // We have moved the world forwards in time
		
	}
	queue.enqueueReadBuffer(buffState, CL_TRUE, 0, cbBuffer, &world.state[0]);
	
}

}; // namespace	bcs10
}; // namespace hpce

int main(int argc, char *argv[])
{
	float dt=0.1;
	unsigned n=1;
	bool binary=false;
	
	if(argc>1){
		dt=(float)strtod(argv[1], NULL);
	}
	if(argc>2){
		n=atoi(argv[2]);
	}
	if(argc>3){
		if(atoi(argv[3]))
			binary=true;
	}
	
	try{
		hpce::world_t world=hpce::LoadWorld(std::cin);
		std::cerr<<"Loaded world with w="<<world.w<<", h="<<world.h<<std::endl;
		
		std::cerr<<"Stepping by dt="<<dt<<" for n="<<n<<std::endl;
		hpce::bcs10::StepWorldV5PackedProperties(world, dt, n);
		
		hpce::SaveWorld(std::cout, world, binary);
	}catch(const std::exception &e){
		std::cerr<<"Exception : "<<e.what()<<std::endl;
		return 1;
	}
		
	return 0;
}