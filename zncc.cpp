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
	unsigned img_width, img_height, img_height_padded, img_width_padded;
	unsigned sampling_step;
	unsigned padding;
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
	occlusion_filling_win_size = 4;

	padding = 16; //The pixel padding that is added to around image

	// INITIALIZE OPENCL
	//get all platforms (drivers)
	std::vector<cl::Platform> all_platforms;
	
	if (writeProgress)
	{
		std::cout << "Start getting platforms.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}
	
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
	if (writeProgress)
	{
		std::cout << "Start getting devices.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}
	//get default device of the default platform
	std::vector<cl::Device> all_devices;
	default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
	if (all_devices.size() == 0) {
		std::cout << " No devices found. Check OpenCL installation!\n";
		exit(1);
	}

	cl::Device default_device = all_devices[0];
	std::cout << "Using device: " << default_device.getInfo<CL_DEVICE_NAME>() << "\n";

	//Printing device info
	std::cout << "Mem type: " << default_device.getInfo<CL_DEVICE_LOCAL_MEM_TYPE>() << "\n";
	std::cout << "Mem size: " << default_device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() << "\n";
	std::cout << "Compute units: " << default_device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << "\n";
	std::cout << "Max clock freq: " << default_device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << "\n";
	std::cout << "Max const buf size: " << default_device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() << "\n";
	std::cout << "Max workgroup size: " << default_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << "\n";

	for (int i = 0; i < default_device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>().size(); i++) {
		std::cout << "Max work item size: " << default_device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[i] << "\n";
	}

	cl::Context context({ default_device });

	if (writeProgress)
	{
		std::cout << "Start Reading Kernels.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

	std::ifstream t("cl_kernel.cl");
	std::stringstream buffer;
	buffer << t.rdbuf();

	std::string kernel_code = buffer.str();

	cl::Program::Sources sources;
	sources.push_back({ kernel_code.c_str(),kernel_code.length() });

	if (writeProgress)
	{
		std::cout << "Start Building.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

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


	img_height_padded = img_height + (2 * padding);
	img_width_padded = img_width + (2 * padding);

	grey_result_size = img_height_padded * img_width_padded;


	// create buffers on the device
	cl::Buffer buffer_r_original_img(context, CL_MEM_READ_ONLY, sizeof(unsigned char) * grey_input_size);
	cl::Buffer buffer_l_original_img(context, CL_MEM_READ_ONLY, sizeof(unsigned char) * grey_input_size);
	cl::Buffer buffer_r_grey_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_result_size);
	cl::Buffer buffer_l_grey_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * grey_result_size);

	//create queue to which we will push commands for the device.
	cl::CommandQueue queue(context, default_device, CL_QUEUE_PROFILING_ENABLE);
	//queue.enqueueWriteBuffer(bu)
	cl::Event kernel_grey_load_event1;
	cl::Event kernel_grey_load_event2;

	std::vector<cl::Event> img_load_events;
	queue.enqueueWriteBuffer(buffer_r_original_img, CL_FALSE, 0, sizeof(unsigned char) * grey_input_size, r_orig_img.data(), 0, &kernel_grey_load_event1);
	queue.enqueueWriteBuffer(buffer_l_original_img, CL_FALSE, 0, sizeof(unsigned char) * grey_input_size, l_orig_img.data(), 0, &kernel_grey_load_event2);
	img_load_events.push_back(kernel_grey_load_event1);
	img_load_events.push_back(kernel_grey_load_event2);
	queue.enqueueBarrierWithWaitList(&img_load_events);
	//Kernel setup
	cl::Kernel kernel_grey_calc = cl::Kernel(program, "resizeAndGreyScaleImg");

	//Right side
	kernel_grey_calc.setArg(0, buffer_r_original_img);
	kernel_grey_calc.setArg(1, buffer_r_grey_img);
	kernel_grey_calc.setArg(2, img_width);
	kernel_grey_calc.setArg(3, sampling_step);
	kernel_grey_calc.setArg(4, padding);

	cl::Event kernel_grey_calc_event1;
	queue.enqueueNDRangeKernel(kernel_grey_calc, cl::NullRange, cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_grey_calc_event1);

	//Left side
	kernel_grey_calc.setArg(0, buffer_l_original_img);
	kernel_grey_calc.setArg(1, buffer_l_grey_img);
	kernel_grey_calc.setArg(2, img_width);
	kernel_grey_calc.setArg(3, sampling_step);
	kernel_grey_calc.setArg(4, padding);

	cl::Event kernel_grey_calc_event2;
	queue.enqueueNDRangeKernel(kernel_grey_calc, cl::NullRange, cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_grey_calc_event2);

	//FILE OUTPUTS, uncomment to print results
	/*
	std::vector <unsigned char> test_output1;
	test_output1.resize(grey_result_size);
	queue.enqueueReadBuffer(buffer_l_grey_img, CL_TRUE, 0, sizeof(unsigned char) * grey_result_size, test_output1.data());

	encodeGreyImg("output/padded_g_image.png", test_output1, img_width_padded, img_height_padded);
*/

	//Mean calculations by opencl
	unsigned pixels = img_width * img_height;

	unsigned pixels_padded = img_height_padded * img_width_padded;

	//Take away everytime performed useless calculation from kernel
	int win_rad = (win_size - 1) / 2;

	cl::Buffer buffer_l_mean_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels_padded);
	cl::Buffer buffer_r_mean_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels_padded);

	//Kernel setup
	cl::Kernel kernel_mean_calc = cl::Kernel(program, "mean_calc");

	//Right side
	kernel_mean_calc.setArg(0, buffer_r_grey_img);
	kernel_mean_calc.setArg(1, buffer_r_mean_img);
	kernel_mean_calc.setArg(2, img_width_padded);
	kernel_mean_calc.setArg(3, img_height_padded);
	kernel_mean_calc.setArg(4, win_rad);

	cl::Event kernel_mean_calc_event1;
	queue.enqueueNDRangeKernel(kernel_mean_calc, cl::NDRange(padding, padding), cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_mean_calc_event1);

	//Left side
	kernel_mean_calc.setArg(0, buffer_l_grey_img);
	kernel_mean_calc.setArg(1, buffer_l_mean_img);
	kernel_mean_calc.setArg(2, img_width_padded);
	kernel_mean_calc.setArg(3, img_height_padded);
	kernel_mean_calc.setArg(4, win_rad);

	cl::Event kernel_mean_calc_event2;
	queue.enqueueNDRangeKernel(kernel_mean_calc, cl::NDRange(padding, padding), cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_mean_calc_event2);
	
	//FILE OUTPUTS, , uncomment to print results
	/*
	std::vector <unsigned char> test_output2;
	test_output2.resize(pixels_padded);
	queue.enqueueReadBuffer(buffer_l_mean_img, CL_TRUE, 0, sizeof(unsigned char) * pixels_padded, test_output2.data());

	encodeGreyImg("output/padded_mean_image.png", test_output2, img_width_padded, img_height_padded);
	*/

	// ZNCC calculations opencl
	const int img_size = img_width*img_height;


	cl::Buffer buffer_r_zncc_result_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels_padded);
	cl::Buffer buffer_l_zncc_result_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels_padded);
	
	//Kernel setup
	cl::Kernel kernel_zncc_calc = cl::Kernel(program, "zncc_calc");


	float scale_factor = 255.0f / -max_disparity;

	//Left-to-right
	kernel_zncc_calc.setArg(0, buffer_l_grey_img);
	kernel_zncc_calc.setArg(1, buffer_r_grey_img);
	kernel_zncc_calc.setArg(2, buffer_l_mean_img);
	kernel_zncc_calc.setArg(3, buffer_r_mean_img);
	kernel_zncc_calc.setArg(4, buffer_l_zncc_result_img);
	kernel_zncc_calc.setArg(5, img_width_padded);
	kernel_zncc_calc.setArg(6, img_height_padded);
	kernel_zncc_calc.setArg(7, win_rad);
	kernel_zncc_calc.setArg(8, -max_disparity);
	kernel_zncc_calc.setArg(9, scale_factor);

	cl::Event kernel_zncc_calc_event1;
	queue.enqueueNDRangeKernel(kernel_zncc_calc, cl::NDRange(padding, padding), cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_zncc_calc_event1);

	scale_factor = 255.0f / max_disparity;

	//Right to left
	kernel_zncc_calc.setArg(0, buffer_r_grey_img);
	kernel_zncc_calc.setArg(1, buffer_l_grey_img);
	kernel_zncc_calc.setArg(2, buffer_r_mean_img);
	kernel_zncc_calc.setArg(3, buffer_l_mean_img);
	kernel_zncc_calc.setArg(4, buffer_r_zncc_result_img);
	kernel_zncc_calc.setArg(5, img_width_padded);
	kernel_zncc_calc.setArg(6, img_height_padded);
	kernel_zncc_calc.setArg(7, win_rad);
	kernel_zncc_calc.setArg(8, max_disparity);
	kernel_zncc_calc.setArg(9, scale_factor);

	cl::Event kernel_zncc_calc_event2;
	queue.enqueueNDRangeKernel(kernel_zncc_calc, cl::NDRange(padding, padding), cl::NDRange(img_width,img_height), cl::NullRange, 0, &kernel_zncc_calc_event2);

	//FILE OUTPUTS, , uncomment to print results
	
/*	std::vector <unsigned char> test_output3;
	test_output3.resize(grey_result_size);
	queue.enqueueReadBuffer(buffer_r_zncc_result_img, CL_TRUE, 0, sizeof(unsigned char) * grey_result_size, test_output3.data());

	encodeGreyImg("output/padded_zncc.png", test_output3, img_width_padded, img_height_padded);
*/

	//Cross checking by opencl

	cl::Buffer buffer_cc_img(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * pixels_padded);

	cl::Kernel kernel_cross_check = cl::Kernel(program, "cross_check");
	kernel_cross_check.setArg(0, buffer_r_zncc_result_img);
	kernel_cross_check.setArg(1, buffer_l_zncc_result_img);
	kernel_cross_check.setArg(2, buffer_cc_img);
	kernel_cross_check.setArg(3, img_width_padded);
	kernel_cross_check.setArg(4, img_height_padded);
	kernel_cross_check.setArg(5, max_diff);

	cl::Event kernel_cc_calc_event;
	queue.enqueueNDRangeKernel(kernel_cross_check, cl::NDRange(padding, padding), cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_cc_calc_event);

	//FILE OUTPUTS, , uncomment to print results
	/*
	std::vector <unsigned char> test_output4;
	test_output4.resize(grey_result_size);
	queue.enqueueReadBuffer(buffer_cc_img, CL_TRUE, 0, sizeof(unsigned char) * grey_result_size, test_output4.data());

	encodeGreyImg("output/padded_cc.png", test_output4, img_width_padded, img_height_padded);
	*/

	//Occlusionfill by opencl

	occlusion_filled_img.resize(pixels_padded);

	cl::Kernel kernel_occlusion_filling = cl::Kernel(program, "occlusion_filling");
	kernel_occlusion_filling.setArg(0, buffer_cc_img);
	kernel_occlusion_filling.setArg(1, img_width_padded);
	kernel_occlusion_filling.setArg(2, img_height_padded);
	kernel_occlusion_filling.setArg(3, occlusion_filling_win_size);

	cl::Event kernel_of_calc_event;
	queue.enqueueNDRangeKernel(kernel_occlusion_filling, cl::NDRange(padding, padding), cl::NDRange(img_width, img_height), cl::NullRange, 0, &kernel_of_calc_event);

	queue.finish();


// Reading data back from the device
	queue.enqueueReadBuffer(buffer_cc_img, CL_TRUE, 0, sizeof(unsigned char) * pixels_padded, occlusion_filled_img.data());


	//https://stackoverflow.com/questions/23070933/timing-execution-of-opencl-kernels
/*
	double img_load1 = kernel_grey_load_event1.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_grey_load_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();
	printf("OpenCl Execution time for grey load 1 is: %0.3f milliseconds \n", img_load1 / 1000000.0);

	double img2_startDelay = kernel_grey_load_event2.getProfilingInfo<CL_PROFILING_COMMAND_START>() - kernel_grey_load_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();
	printf("OpenCl Wait time for grey load 2 is: %0.3f milliseconds \n", img2_startDelay / 1000000.0);
	*/
	double grey_calc1 = kernel_grey_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_grey_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();
	double grey_calc2 = kernel_grey_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_grey_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	double mean_calc1 = kernel_mean_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_mean_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();
	double mean_calc2 = kernel_mean_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_mean_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	double zncc_calc1 = kernel_zncc_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_zncc_calc_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();
	double zncc_calc2 = kernel_zncc_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_zncc_calc_event2.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	double cc_calc = kernel_cc_calc_event.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_cc_calc_event.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	double of_calc = kernel_of_calc_event.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_of_calc_event.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	double total_transfer_and_calculation_time = kernel_of_calc_event.getProfilingInfo<CL_PROFILING_COMMAND_END>() - kernel_grey_load_event1.getProfilingInfo<CL_PROFILING_COMMAND_START>();

	printf("OpenCl Execution time for grey calc 1 is: %0.3f milliseconds \n", grey_calc1 / 1000000.0);
	printf("OpenCl Execution time for grey calc 2 is: %0.3f milliseconds \n", grey_calc2 / 1000000.0);

	printf("OpenCl Execution time for mean calc 1 is: %0.3f milliseconds \n", mean_calc1 / 1000000.0);
	printf("OpenCl Execution time for mean calc 2 is: %0.3f milliseconds \n", mean_calc2 / 1000000.0);

	printf("OpenCl Execution time for zncc calc 1 is: %0.3f milliseconds \n", zncc_calc1 / 1000000.0);
	printf("OpenCl Execution time for zncc calc 2 is: %0.3f milliseconds \n", zncc_calc2 / 1000000.0);

	printf("OpenCl Execution time for cross check calc is: %0.3f milliseconds \n", cc_calc / 1000000.0);
	printf("OpenCl Execution time for occlusion filling calc is: %0.3f milliseconds \n", of_calc / 1000000.0);

	printf("OpenCl Execution sum is: %0.3f milliseconds \n", (grey_calc1+grey_calc2+mean_calc1+mean_calc2+zncc_calc1+zncc_calc2+cc_calc+of_calc) / 1000000.0);

	printf("GPU time from first load to last kernel stops is: %0.3f milliseconds \n", total_transfer_and_calculation_time / 1000000.0);
	
	if (write_post_of_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding occlusion filled image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}


		encodeGreyImg(post_of_file, occlusion_filled_img, img_width_padded, img_height_padded);
	}

	if (writeProgress)
	{
		std::cout << "Finished.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

	system("pause");
    return 0;
}

