// zncc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <thread>


void startCounter(double &PCFreq, __int64 &CounterStart)
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		std::cout << "QueryPerformanceFrequency failed!\n";

	PCFreq = double(li.QuadPart) / 1000.0;

	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}

double getCounter(double &PCFreq, __int64 &CounterStart)
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}


bool encodeGreyImg(const std::string &filename, std::vector<unsigned char>& image, unsigned width, unsigned height)
{
	//Encode the image
	unsigned error = lodepng::encode(filename, image, width, height, LodePNGColorType::LCT_GREY);

	//if there's an error, display it
	if (error)
	{
		std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
		return false;
	}
	return true;
}



// Method is based on example_decode.cpp from lodepng examples https://raw.githubusercontent.com/lvandeve/lodepng/master/examples/example_decode.cpp
void decodeImg(const std::string &filename, std::vector<unsigned char> &image, unsigned &width, unsigned &height, bool &success)
{
	unsigned int error = lodepng::decode(image, width, height, filename);
	//if there's an error, display it - else show size of image
	if (error)
	{
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
		success = false;
	}
	else 
	{ 
		success = true;
	}
	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
}


int main(int argc, char *argv[])
{	
	// usage of queryperformancecounter is based on  https://stackoverflow.com/questions/1739259/how-to-use-queryperformancecounter
	double PCFreq = 0.0;
	__int64 CounterStart = 0;

	startCounter(PCFreq, CounterStart);

	std::vector<unsigned char> l_orig_img;
	std::vector<unsigned char> r_orig_img;
	std::vector <unsigned char> occlusion_filled_img;

	std::string l_file;
	std::string r_file;
	std::string grey_l_file, grey_r_file, mean_l_file, mean_r_file, depth_l_file, depth_r_file, post_file, post_of_file;
	unsigned img_width, img_height;
	unsigned sampling_step;
	int win_size, max_disparity, max_diff, occlusion_filling_win_size;
	const int zncc_workers = 16;
	const int of_workers = 4;

	unsigned l_width, l_height, r_width, r_height; // to check that images have same width and height
	bool write_grey_scale_img_to_file;
	bool write_mean_value_img_to_file;
	bool write_pre_depth_img_to_file;
	bool write_post_img_to_file;
	bool write_post_of_img_to_file;
	bool writeProgress; 


	// hard coded values - TODO: argument reader and real values
	write_post_of_img_to_file = true;
	sampling_step = 4;
	writeProgress = true;
	post_of_file = "output/depthmap_of.png";
	win_size = 9; // should be 9
	max_disparity = 65;  // should be 65
	max_diff = 8;
	occlusion_filling_win_size = 8;

	// INITIALIZE OPENCL
	//get all platforms (drivers)
	std::vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);
	if (all_platforms.size() == 0) {
		std::cout << " No platforms found. Check OpenCL installation!\n";
		exit(1);
	}
	cl::Platform default_platform;
	default_platform = all_platforms[0];
	if (all_platforms.size() > 1)
	{
		default_platform = all_platforms[1];
	}
	
	std::cout << "Using platform: " << default_platform.getInfo<CL_PLATFORM_NAME>() << "\n";

	//get default device of the default platform
	std::vector<cl::Device> all_devices;
	default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
	if (all_devices.size() == 0) {
		std::cout << " No devices found. Check OpenCL installation!\n";
		exit(1);
	}
	cl::Device default_device = all_devices[0];
	std::cout << "Using device: " << default_device.getInfo<CL_DEVICE_NAME>() << "\n";

	cl::Context context({ default_device });

	std::ifstream t("cl_kernel.cl");
	std::stringstream buffer;
	buffer << t.rdbuf();

	// kernel calculates for each element C=A+B
	std::string kernel_code = buffer.str();

	cl::Program::Sources sources;
	sources.push_back({ kernel_code.c_str(),kernel_code.length() });

	cl::Program program(context, sources);
	if (program.build({ default_device }) != CL_SUCCESS) {
		std::cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
		system("pause");
		exit(1);
	}

	// READ IMAGE
	// if arguments - try to read such a files
	if (argc == 2)
	{
		l_file = *argv[1];
		r_file = *argv[2];
	}
	else
	{
		l_file = "images/im0.png";
		r_file = "images/im1.png";
	}

	if (writeProgress)
	{
		std::cout << "Reading images.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}
	bool success1 = false;
	bool success2 = false;

	std::thread decodeThread1(decodeImg, l_file, std::ref(l_orig_img), std::ref(l_width), std::ref(l_height), std::ref(success1));
	decodeImg(r_file, r_orig_img, r_width, r_height, success2);
	decodeThread1.join();

	if (!(success1
		&& success2))
	{
		std::cout << "read failed, exiting" << std::endl;
		system("pause");
		return 1;
	}

	// check that image size match
	if (l_width != r_width || l_height != r_height)
	{
		std::cout << "different sized images, exiting" << std::endl;
		system("pause");
		return 2;
	}

	img_height = l_height;
	img_width = l_width;

	if (writeProgress)
	{
		std::cout << "Resizing images.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}


	//resizeAndGreyScaleImg by opencl
	unsigned grey_input_size = (img_width * img_height) * 4; // 4 stands for rgba
	unsigned grey_result_size = grey_input_size / (sampling_step*sampling_step*4);

	// images are same sized - use img_height as common name
	img_height = img_height / sampling_step;
	img_width = img_width / sampling_step;

	// create buffers on the device
	cl::Buffer buffer_r_original_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_input_size);
	cl::Buffer buffer_l_original_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_input_size);
	cl::Buffer buffer_r_grey_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_result_size);
	cl::Buffer buffer_l_grey_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_result_size);

	//create queue to which we will push commands for the device.
	cl::CommandQueue queue(context, default_device);

	queue.enqueueWriteBuffer(buffer_r_original_img, CL_TRUE, 0, sizeof(unsigned char) * grey_input_size, r_orig_img.data());
	queue.enqueueWriteBuffer(buffer_l_original_img, CL_TRUE, 0, sizeof(unsigned char) * grey_input_size, l_orig_img.data());

	//Kernel setup
	cl::Kernel kernel_grey_calc = cl::Kernel(program, "resizeAndGreyScaleImg");

	//Right side
	kernel_grey_calc.setArg(0, buffer_r_original_img);
	kernel_grey_calc.setArg(1, buffer_r_grey_img);
	kernel_grey_calc.setArg(2, img_width);
	kernel_grey_calc.setArg(3, sampling_step);
	queue.enqueueNDRangeKernel(kernel_grey_calc, cl::NullRange, cl::NDRange(grey_result_size), cl::NullRange);

	//Left side
	kernel_grey_calc.setArg(0, buffer_l_original_img);
	kernel_grey_calc.setArg(1, buffer_l_grey_img);
	kernel_grey_calc.setArg(2, img_width);
	kernel_grey_calc.setArg(3, sampling_step);
	queue.enqueueNDRangeKernel(kernel_grey_calc, cl::NullRange, cl::NDRange(grey_result_size), cl::NullRange);

	
	//Mean calculations by opencl
	unsigned pixels = img_width * img_height;

	cl::Buffer buffer_l_mean_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);
	cl::Buffer buffer_r_mean_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);

	//Kernel setup
	cl::Kernel kernel_mean_calc = cl::Kernel(program, "mean_calc");

	//Right side
	kernel_mean_calc.setArg(0, buffer_r_grey_img);
	kernel_mean_calc.setArg(1, buffer_r_mean_img);
	kernel_mean_calc.setArg(2, img_width);
	kernel_mean_calc.setArg(3, img_height);
	kernel_mean_calc.setArg(4, win_size);
	queue.enqueueNDRangeKernel(kernel_mean_calc, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	//Left side
	kernel_mean_calc.setArg(0, buffer_l_grey_img);
	kernel_mean_calc.setArg(1, buffer_l_mean_img);
	kernel_mean_calc.setArg(2, img_width);
	kernel_mean_calc.setArg(3, img_height);
	kernel_mean_calc.setArg(4, win_size);
	queue.enqueueNDRangeKernel(kernel_mean_calc, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	if (writeProgress)
	{
		std::cout << "Starting zncc 1/2..." << getCounter(PCFreq, CounterStart) << std::endl;
	}
	
	// ZNCC calculations opencl

	cl::Buffer buffer_r_zncc_result_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);
	cl::Buffer buffer_l_zncc_result_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);
	
	//Kernel setup
	cl::Kernel kernel_zncc_calc = cl::Kernel(program, "zncc_calc");

	//Left-to-right
	kernel_zncc_calc.setArg(0, buffer_l_grey_img);
	kernel_zncc_calc.setArg(1, buffer_r_grey_img);
	kernel_zncc_calc.setArg(2, buffer_l_mean_img);
	kernel_zncc_calc.setArg(3, buffer_r_mean_img);
	kernel_zncc_calc.setArg(4, buffer_l_zncc_result_img);
	kernel_zncc_calc.setArg(5, img_width);
	kernel_zncc_calc.setArg(6, img_height);
	kernel_zncc_calc.setArg(7, win_size);
	kernel_zncc_calc.setArg(8, -max_disparity);
	queue.enqueueNDRangeKernel(kernel_zncc_calc, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	//Right to left
	kernel_zncc_calc.setArg(0, buffer_r_grey_img);
	kernel_zncc_calc.setArg(1, buffer_l_grey_img);
	kernel_zncc_calc.setArg(2, buffer_r_mean_img);
	kernel_zncc_calc.setArg(3, buffer_l_mean_img);
	kernel_zncc_calc.setArg(4, buffer_r_zncc_result_img);
	kernel_zncc_calc.setArg(5, img_width);
	kernel_zncc_calc.setArg(6, img_height);
	kernel_zncc_calc.setArg(7, win_size);
	kernel_zncc_calc.setArg(8, max_disparity);
	queue.enqueueNDRangeKernel(kernel_zncc_calc, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	//Cross checking by opencl

	cl::Buffer buffer_cc_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);

	cl::Kernel kernel_cross_check = cl::Kernel(program, "cross_check");
	kernel_cross_check.setArg(0, buffer_r_zncc_result_img);
	kernel_cross_check.setArg(1, buffer_l_zncc_result_img);
	kernel_cross_check.setArg(2, buffer_cc_img);
	kernel_cross_check.setArg(3, img_width);
	kernel_cross_check.setArg(4, img_height);
	kernel_cross_check.setArg(5, max_diff);
	queue.enqueueNDRangeKernel(kernel_cross_check, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	//Occlusionfill by opencl

	occlusion_filled_img.resize(pixels);

	//cl::Buffer buffer_post_processed_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels);

	cl::Kernel kernel_occlusion_filling = cl::Kernel(program, "occlusion_filling");
	kernel_occlusion_filling.setArg(0, buffer_cc_img);
	//kernel_occlusion_filling.setArg(1, buffer_post_processed_img);
	kernel_occlusion_filling.setArg(1, img_width);
	kernel_occlusion_filling.setArg(2, img_height);
	kernel_occlusion_filling.setArg(3, occlusion_filling_win_size);
	queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);

	//Using smaller window and multiple re-runs could cause better quality results
	queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);
	//queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);
	//queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);
	//queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);
	//queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NullRange, cl::NDRange(pixels), cl::NullRange);
	queue.finish();

	// Reading data back from the device
	queue.enqueueReadBuffer(buffer_cc_img, CL_TRUE, 0, sizeof(unsigned char) * pixels, occlusion_filled_img.data());

	if (write_post_of_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding occlusion filled image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}


		encodeGreyImg(post_of_file, occlusion_filled_img, img_width, img_height);
	}

	if (writeProgress)
	{
		std::cout << "Finished.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

	system("pause");
    return 0;
}

