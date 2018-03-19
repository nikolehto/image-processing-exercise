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


void resizeAndGreyScaleImg(std::vector<unsigned char> &img
	, unsigned &width
	, unsigned &height
	, const unsigned sampling_step)
{
	std::vector<unsigned char> newImage;
	newImage.reserve(width*height);

	//const unsigned char r_con = 0.2126 * 255, g_con = 0.7152 * 255, b_con = 0.0722 * 255; // TODO (for performance somehow use char? maybe output should be shifted or something) 
	const float r_con = 0.2126f, g_con = 0.7152f, b_con = 0.0722f;
	
	const unsigned x_step = sampling_step * 4; // 4 channels
	const unsigned widthc4 = width * 4; // there are actually 4 * width values in x axis

	for (unsigned img_y = 0; img_y < height; img_y += sampling_step)
	{
		const unsigned y_offset = img_y * widthc4;
		for (unsigned img_x = 0; img_x < widthc4; img_x += x_step)
		{
			const unsigned index = y_offset + img_x;
			newImage.push_back(roundf(img.at(index) *r_con
				+ img.at(index + 1) * g_con
				+ img.at(index + 2) * b_con));
		}

	}

	height /= sampling_step;
	width /= sampling_step;

	img.swap(newImage); 
}

void createMeanImg(std::vector<unsigned char> &img // parameter img
	, std::vector<unsigned char> &mean_img // return img
	, unsigned &img_width
	, unsigned &img_height
	, unsigned win_size)
{
	mean_img.reserve(img_width*img_height);

	const int win_rad_x = (win_size -1) / 2;
	const int win_rad_y = (win_size -1) / 2;

	for (int img_y = 0; img_y < img_height; img_y++)
	{
		for (int img_x = 0; img_x < img_width; img_x++)
		{
			unsigned int mean = 0;
			unsigned int value_count = 0;

			for (int win_y = -win_rad_y; win_y <= win_rad_y; win_y++)
			{
				const int y_index = img_y + win_y;
				if (y_index < 0 || y_index >= img_height)
				{
					continue;
				}
				const int y_offset = y_index * img_width;

				for (int win_x = -win_rad_x; win_x <= win_rad_x; win_x++)
				{
					const int x_index = img_x + win_x;
					if (x_index < 0 || x_index >= img_width)
					{
						continue;
					}
					const unsigned index = y_offset + x_index;
					mean += img.at(index);
					value_count++;
				}
			}
			// no need to zero test, since index+win_x and index_win_y always exists
			mean /= value_count;

			mean_img.push_back((unsigned char) mean);
		}
	}

}

void crossCheck(std::vector<unsigned char> &l_img // parameter
	, std::vector<unsigned char> &r_img // parameter 
	, std::vector<unsigned char> &post_img // return value
	, unsigned img_width
	, unsigned img_height
	, int max_diff)
{
	unsigned image_size = img_width * img_height;
	post_img.reserve(image_size);
	for (unsigned index = 0; index < img_width * img_height; index++)
	{
		const int diff = l_img.at(index) - r_img.at(index);
		if (diff >= 0) // l_img is winner
		{
			if (diff <= max_diff)
			{
				post_img.push_back(l_img.at(index));
			}
			else
			{
				post_img.push_back(0);
			}
		}
		else //  r_img is winner
		{
			if (-diff <= max_diff)
			{
				post_img.push_back(r_img.at(index));
			}
			else
			{
				post_img.push_back(0);
			}

		}
	}
}

void occlusionFilling(std::vector<unsigned char> &post_img
	, std::vector<unsigned char> &of_img // return value
	, unsigned img_width
	, unsigned img_height
	, int window_radius
	, unsigned int height_start
	, unsigned int height_stop)
{
	//unsigned image_size = img_width * img_height;
	//of_img.reserve(image_size);

	//for (int img_y = 0; img_y < img_height; img_y++)
	for (int img_y = height_start; img_y < height_stop; img_y++)
	{
		int y_offset = img_y * img_width;
		for (int img_x = 0; img_x < img_width; img_x++)
		{
			if (post_img.at(y_offset + img_x) == 0)
			{
				//calculates mean value from window pixels but ignoring zero valued pixels and using also values that are calculated previous rounds
				//window_radius = 1 => 9 tiles
				//window_radius = 2 => 25 tiles
				//window_radius = 3 => 49 tiles
				//window_radius = 4 => 81 tiles

				int window_sum = 0;
				int window_valid_pixels_count = 0;


				for (int window_y = -window_radius; window_y <= window_radius; window_y++)
				{
					if ((img_y + window_y) < 0)
					{
						//not valid
						continue;
					}

					if ((img_y + window_y) >= img_height)
					{
						//not valid
						continue;
					}

					for (int window_x = -window_radius; window_x <= window_radius; window_x++)
					{
						//check if valid window pixel
						if ((img_x + window_x) < 0)
						{
							//not valid
							continue;
						}

						if ((img_x + window_x) >= img_width)
						{
							//not valid
							continue;
						}

						//pixel location is valid

						int current_pixel_index = y_offset + (window_y * img_width) + (img_x + window_x);

						if (post_img.at(current_pixel_index) != 0)
						{
							window_sum = window_sum + post_img.at(current_pixel_index);
							window_valid_pixels_count++;
						}
						else
						{
							//get value of pixel from calculated result of previous rounds
							if ((y_offset + img_x) > (current_pixel_index))
							{
								//get value of previously calculated result pixel
								if (of_img.at(current_pixel_index) != 0)
								{
									window_sum = window_sum + of_img.at(current_pixel_index);
									window_valid_pixels_count++;
								}
							}
						}
					}
				}

				//Calculate mean and insert it to result picture	
				if (window_valid_pixels_count > 0)
				{
					int mean = window_sum / window_valid_pixels_count;
					//of_img.push_back(mean);
					of_img.at(y_offset + img_x) = mean;
				}
				else
				{
					//of_img.push_back(0);
					of_img.at(y_offset + img_x) = 0;
				}

			}
			else
			{
				//of_img.push_back(post_img.at(y_offset + img_x));
				of_img.at(y_offset + img_x) = post_img.at(y_offset + img_x);
			}
		}
	}
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

void calc_zncc(std::vector<unsigned char> &l_img // parameter
	, std::vector<unsigned char> &r_img // parameter 
	, std::vector<unsigned char> &l_mean_img // parameter 
	, std::vector<unsigned char> &r_mean_img // parameter
	, std::vector<unsigned char> &disp_img // return value
	, unsigned img_width
	, unsigned img_height
	, unsigned height_start
	, unsigned height_stop
	, unsigned win_size
	, int max_disparity
	, bool writeProgress)
{
	const int win_rad_x = (win_size - 1) / 2;
	const int win_rad_y = (win_size - 1) / 2;
	
	if (max_disparity == 0)
	{
		std::cout << "max_disparity cannot be zero" << std::endl;
		return;
	}

	int min_disparity = 0;
	const float scale_factor = (float) 255 / (max_disparity - min_disparity); //scale factor may be negative, at least in c++ its ok to save negative values to unsigned (later scale_factor*d_index is casted to unsigned char)
	if (max_disparity < 0) // if max_disparity is negative swap min_disparity and max_disparity
	{
		min_disparity = max_disparity;
		max_disparity = 0;
	}

	const float printProgressCounter = (height_stop - height_start) / (float) 100;
	const int printProgressCounterStep = roundf(printProgressCounter * 20); //  Print progress each 20th %

	for (int img_y = height_start; img_y < height_stop; img_y++)	
	{
		if (writeProgress)
		{
			if ((img_y - height_start) % (printProgressCounterStep)== 0)
			{
				std::cout << "Progress " << (img_y - height_start) / printProgressCounter << "% \n" ;
			}
		}

		for (int img_x = 0; img_x < img_width; img_x++)
		{
			const int l_mean_value = l_mean_img.at(img_y * img_width + img_x);

			int winner_d_index = 0;
			double highest_correlation = 0.0f;

			for (int disp_x = min_disparity; disp_x< max_disparity; disp_x++)
			{
				if (img_x + disp_x < 0 || img_x + disp_x >= img_width)
				{
					continue;
				}

				int upper_sum = 0;
				int lower_sum_0 = 0;
				int lower_sum_1 = 0;

				const int r_mean_value = r_mean_img.at(img_y * img_width + img_x + disp_x);

				for (int win_y = -win_rad_y; win_y <= win_rad_y; win_y++)
				{
					const int y_index = img_y + win_y;
					if (y_index < 0 || y_index >= img_height)
					{
						continue;
					}
					const int y_offset = y_index * img_width;

					for (int win_x = -win_rad_x; win_x <= win_rad_x; win_x++)
					{ 
						const int x_index = img_x + win_x;
						const int x_disp_index = img_x + win_x + disp_x;
						if (x_disp_index < 0 
							|| x_disp_index >= img_width
							|| x_index < 0
							|| x_index >= img_width)
						{
							// how to deal with edges, if continue does it have any functionality or misfunctionality
							continue;
						}

						// use [] operator since it faster than .at() operator (boundaries are checked previously)
						const int left_pixel_val_diff_from_avg = l_img[y_offset + x_index] - l_mean_value;
						const int right_pixel_val_diff_from_avg = r_img[y_offset + x_disp_index] - r_mean_value;
						upper_sum += left_pixel_val_diff_from_avg * right_pixel_val_diff_from_avg;
						lower_sum_0 += left_pixel_val_diff_from_avg * left_pixel_val_diff_from_avg;
						lower_sum_1 += right_pixel_val_diff_from_avg * right_pixel_val_diff_from_avg;
					}
				}
				const double lower_value = (sqrt(lower_sum_0) * sqrt(lower_sum_1));
				if (lower_value!= 0.0)
				{
					const double zncc_value = upper_sum / lower_value;
					if (zncc_value > highest_correlation)
					{
						highest_correlation = zncc_value;
						winner_d_index = disp_x;
					}
				}
			}
			disp_img.at(img_y * img_width + img_x) = roundf(winner_d_index * scale_factor);
		}
	}
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


/* Based on
http://simpleopencl.blogspot.fi/2013/06/tutorial-simple-start-with-opencl-and-c.html
*/
void simpleAdd(cl::Context context, cl::Device default_device, cl::Program program)
{
	// create buffers on the device
	cl::Buffer buffer_A(context, CL_MEM_READ_WRITE, sizeof(int) * 10);
	cl::Buffer buffer_B(context, CL_MEM_READ_WRITE, sizeof(int) * 10);
	cl::Buffer buffer_C(context, CL_MEM_READ_WRITE, sizeof(int) * 10);

	int A[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	int B[] = { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0 };

	//create queue to which we will push commands for the device.
	cl::CommandQueue queue(context, default_device);

	//cl::KernelFunctor simple_add(cl::Kernel(program, "simple_add"), queue, cl::NullRange, cl::NDRange(10), cl::NullRange);
	//write arrays A and B to the device
	queue.enqueueWriteBuffer(buffer_A, CL_TRUE, 0, sizeof(int) * 10, A);
	queue.enqueueWriteBuffer(buffer_B, CL_TRUE, 0, sizeof(int) * 10, B);

	cl::Kernel kernel_add = cl::Kernel(program, "simple_add");
	kernel_add.setArg(0, buffer_A);
	kernel_add.setArg(1, buffer_B);
	kernel_add.setArg(2, buffer_C);
	queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(10), cl::NullRange);
	queue.finish();
	
	int C[10];
	//read result C from the device to array C
	queue.enqueueReadBuffer(buffer_C, CL_TRUE, 0, sizeof(int) * 10, C);

	std::cout << " result: ";
	for (int i = 0; i<10; i++) {
		std::cout << C[i] << " ";
	}
	std::cout << "\n";
}

// Slower to transfer image-data to gpu than do resize on cpu !?
void clResizeAndGreyScale(std::vector<unsigned char> &img
	, unsigned &width
	, unsigned &height
	, const unsigned sampling_step
	, cl::Context context
	, cl::Device default_device
	, cl::Program program)
{
	// using images or just char vectors?
	// NOT SURE ABOUT FUNCTIONALITY ( img[0] is just a guess how to pass reference of vector )
	cl::Image2D clImageL = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_R, CL_CHAR_BIT), width, height, img[0]);

}

int main(int argc, char *argv[])
{	
	// usage of queryperformancecounter is based on  https://stackoverflow.com/questions/1739259/how-to-use-queryperformancecounter
	double PCFreq = 0.0;
	__int64 CounterStart = 0;

	startCounter(PCFreq, CounterStart);

	std::vector<unsigned char> l_img; 
	std::vector<unsigned char> r_img; 
	std::vector<unsigned char> l_mean_img;
	std::vector<unsigned char> r_mean_img;
	std::vector <unsigned char> l_disp_img;
	std::vector <unsigned char> r_disp_img;
	std::vector <unsigned char> post_processed_img;
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
	write_grey_scale_img_to_file = false;
	write_mean_value_img_to_file = false;
	write_pre_depth_img_to_file = false;
	write_post_img_to_file = false;
	write_post_of_img_to_file = true;
	sampling_step = 4;
	writeProgress = true;
	grey_l_file = "output/grey_im0.png";
	grey_r_file = "output/grey_im1.png";
	mean_l_file = "output/mean_im0.png";
	mean_r_file = "output/mean_im1.png";
	depth_l_file = "output/pre_depth_im0.png";
	depth_r_file = "output/pre_depth_im1.png";
	post_file = "output/depthmap.png";
	post_of_file = "output/depthmap_of.png";
	win_size = 9; // should be 9
	max_disparity = 65;  // should be 65
	max_diff = 8;
	occlusion_filling_win_size = 3;

	// INITIALIZE OPENCL
	//get all platforms (drivers)
	std::vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);
	if (all_platforms.size() == 0) {
		std::cout << " No platforms found. Check OpenCL installation!\n";
		exit(1);
	}
	cl::Platform default_platform = all_platforms[0];
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
		exit(1);
	}

	simpleAdd(context, default_device, program); // TEST function todo remove

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

	std::thread decodeThread1(decodeImg, l_file, std::ref(l_img), std::ref(l_width), std::ref(l_height), std::ref(success1));
	decodeImg(r_file, r_img, r_width, r_height, success2);
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

	if (writeProgress)
	{
		std::cout << "Resizing images.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

	clResizeAndGreyScale(l_img, l_width, l_height, sampling_step, context, default_device, program);

	// resize and grayscale images one by one
	std::thread resizeAndGreyScaleThread(resizeAndGreyScaleImg, std::ref(l_img), std::ref(l_width), std::ref(l_height), sampling_step);
	resizeAndGreyScaleImg(r_img, r_width, r_height, sampling_step);
	resizeAndGreyScaleThread.join();

	// images are same sized - use img_height as common name
	img_height = l_height;
	img_width = l_width;

	if (write_grey_scale_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding grey images.. " << std::endl;
		}

		// Encoding and writing grey images to file parallel
		std::thread encodeGreyThread(encodeGreyImg, grey_l_file, l_img, img_width, img_height);
		encodeGreyImg(grey_r_file, r_img, img_width, img_height);
		encodeGreyThread.join();

	}

	if (writeProgress)
	{
		std::cout << "Creating mean calculations.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}

	// calculate meanIMgs parallel
	std::thread meanThread(createMeanImg, std::ref(r_img), std::ref(r_mean_img), std::ref(r_width), std::ref(r_height), win_size);
	createMeanImg(l_img, l_mean_img, l_width, l_height, win_size);
	meanThread.join();

	if (write_mean_value_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding mean calculations to image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}
		encodeGreyImg(mean_l_file, l_mean_img, img_width, img_height);
		encodeGreyImg(mean_r_file, r_mean_img, img_width, img_height);
	}
	

	if (writeProgress)
	{
		std::cout << "Starting zncc 1/2..." << getCounter(PCFreq, CounterStart) << std::endl;
	}

	l_disp_img.resize(img_width*img_height);
	r_disp_img.resize(img_width*img_height);

	// Executing zncc parallel with predefined number of threads

	std::thread znccThreads1[zncc_workers];

	// Dividing image to blocks
	unsigned int start_indexes[zncc_workers];
	unsigned int stop_indexes[zncc_workers];
	float block = img_height / (float) zncc_workers;

	for (int i = 0; i < zncc_workers; i++)
	{
		start_indexes[i] = roundf(i * block);
		stop_indexes[i] = roundf(((i+1) * block));
	}

	for (int i = 0; i < zncc_workers; i++)
	{
		bool temp_writeProgress = false;
		if (i == 0)
		{
			temp_writeProgress = writeProgress;
		}
		
		znccThreads1[i] = std::thread(calc_zncc, l_img, r_img, l_mean_img, r_mean_img, std::ref(l_disp_img), img_width, img_height, start_indexes[i], stop_indexes[i], win_size, -max_disparity, temp_writeProgress);
	}

	for (int i = 0; i < zncc_workers; i++) {
		znccThreads1[i].join();
	}

	// left-right calculation use negative disparity 

	if (writeProgress)
	{
		std::cout << "Starting zncc 2/2..." << getCounter(PCFreq, CounterStart) << std::endl;
	}

	for (int i = 0; i < zncc_workers; i++)
	{
		bool temp_writeProgress = false;
		if (i == 0)
		{
			temp_writeProgress = writeProgress;
		}

		znccThreads1[i] = std::thread(calc_zncc, r_img, l_img, r_mean_img, l_mean_img, std::ref(r_disp_img), img_width, img_height, start_indexes[i], stop_indexes[i], win_size, max_disparity, temp_writeProgress);
	}

	for (int i = 0; i < zncc_workers; i++) {
		znccThreads1[i].join();
	}

	// for right-left use positive disparity value

	if (write_pre_depth_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding pre_depth calculations to image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}


		encodeGreyImg(depth_l_file, l_disp_img, img_width, img_height);
		encodeGreyImg(depth_r_file, r_disp_img, img_width, img_height);
	}

	crossCheck(l_disp_img, r_disp_img, post_processed_img, img_width, img_height, max_diff);

	if (write_post_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding post processed image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}


		encodeGreyImg(post_file, post_processed_img, img_width, img_height);
	}

	occlusion_filled_img.resize(img_width*img_height);


	// Executing occlusion filling with predefined number of threads

	// NOTICE: Best (original) quality result is achieved with 1 thread
	// Using more threads will make stripes/inconsistents appear to end picture
	// This is because algorithm uses intermediate results and cutting image to smaller pieces make edges of pieces be inconsistent with each other
	std::thread ofThreads[of_workers];

	unsigned int of_start_indexes[of_workers];
	unsigned int of_stop_indexes[of_workers];
	float of_block = img_height / (float)of_workers;

	for (int i = 0; i < of_workers; i++)
	{
		of_start_indexes[i] = roundf(i * of_block);
		of_stop_indexes[i] = roundf(((i + 1) * of_block));
	}

	for (int i = 0; i < of_workers; i++)
	{
		ofThreads[i] = std::thread(occlusionFilling, post_processed_img, std::ref(occlusion_filled_img), img_width, img_height, occlusion_filling_win_size, of_start_indexes[i], of_stop_indexes[i]);
	}

	for (int i = 0; i < of_workers; i++) {
		ofThreads[i].join();
	}

	//occlusionFilling(post_processed_img, occlusion_filled_img, img_width, img_height, occlusion_filling_win_size);

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

