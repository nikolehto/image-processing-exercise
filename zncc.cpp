// zncc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"



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
	, unsigned sampling_step)
{
	std::vector<unsigned char> newImage;

	//const unsigned char r_con = 0.2126 * 255, g_con = 0.7152 * 255, b_con = 0.0722 * 255; // TODO (for performance somehow use char? maybe output should be shifted or something) 
	float r_con = 0.2126f, g_con = 0.7152f, b_con = 0.0722f;

	for (unsigned row = 0; row < height; row += sampling_step)
	{
		for (unsigned column = 0; column < width; column += sampling_step)
		{
			const unsigned index = row * width * 4 + column * 4;

			newImage.push_back(img.at(index) *r_con
				+ img.at(index + 1) * g_con
				+ img.at(index + 2) * b_con);
		}
	}

	height /= sampling_step;
	width /= sampling_step;

	img.swap(newImage); 
}

void createMeanImg(std::vector<unsigned char> &img // pass img
	, std::vector<unsigned char> &mean_img // return img
	, unsigned &img_width
	, unsigned &img_height
	, unsigned win_size)
{
	const int win_rad_x = (win_size -1) / 2;
	const int win_rad_y = (win_size -1) / 2;

	for (int img_y = 0; img_y < img_height; img_y++)
	{
		for (int img_x = 0; img_x < img_width; img_x++)
		{
			unsigned int mean = 0;
			unsigned int value_count = 0;

			for (int win_y = -win_rad_y; win_y <= win_rad_y; win_y++) // TODO check <=
			{
				if (img_y + win_y < 0 || img_y + win_y >= img_height) // TODO check >=
				{
					continue;
				}

				for (int win_x = -((int)win_rad_x); win_x <= win_rad_x; win_x++)
				{
					if (img_x + win_x < 0 || img_x + win_x >= img_width) // TODO check >=
					{
						continue;
					}
					const unsigned index = (img_y + win_y) * img_width + img_x + win_x;
					mean += img.at(index);
					value_count++;
				}
			}

			mean /= value_count;

			mean_img.push_back((unsigned char) mean);
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


void calc_zncc(std::vector<unsigned char> &l_img
	, std::vector<unsigned char> &r_img
	, std::vector<unsigned char> &l_mean_img
	, std::vector<unsigned char> &r_mean_img
	, unsigned &img_width
	, unsigned &img_height
	, unsigned &win_size
	, unsigned &max_disparity
	, bool writeProgress)
{
	// TODO algorithm 
	float l_disp_map_max = 0;
	std::vector<float> l_disp_map; 
	std::vector<float> r_disp_map; 


	std::vector<unsigned char> l_disp_img; // TODO - pass as parameter

	const int win_rad_x = (win_size - 1) / 2;
	const int win_rad_y = (win_size - 1) / 2;

	int printProgressCounter = img_height / 100;

	for (int img_y = 0; img_y < img_height; img_y++)
	{
		if (writeProgress)
		{
			if (img_y % printProgressCounter == 0)
			{
				std::cout << "Progress " << img_y / printProgressCounter << "%" << std::endl;
			}
		}

		for (int img_x = 0; img_x < img_width; img_x++)
		{
			const int base_index = img_y * img_width + img_x;

			int upper_sum = 0;
			int lower_sum_0 = 0;
			int lower_sum_1 = 0;

			for (int disp_x = 0; disp_x< max_disparity; disp_x++)
			{
				const int base_disp_index = base_index + disp_x;

				if (img_x + disp_x < 0 || img_x + disp_x >= img_width) // TODO check >=
				{
					continue;
				}

				for (int win_y = -win_rad_y; win_y <= win_rad_y; win_y++) // TODO check <=
				{
					if (img_y + win_y < 0 || img_y + win_y >= img_height) // TODO check >=
					{
						continue;
					}

					for (int win_x = -((int)win_rad_x); win_x <= win_rad_x; win_x++)
					{ 
						if (img_x + win_x + disp_x < 0 
							|| img_x + win_x + disp_x >= img_width
							|| img_x + win_x < 0
							|| img_x + win_x >= img_width) // TODO check >=
						{
							// how to deal with edges, if continue does it have any functionality or misfunctionality
							continue;
						}
						const int win_index = (img_y + win_y) * img_width + img_x + win_x;
						const int win_disp_index = (img_y + win_y) * img_width + img_x + win_x + disp_x;

						// TODO get average value from outer loop ( it is same for all window indexes )

						const int left_pixel_val_diff_from_avg = l_img.at(win_index) - l_mean_img.at(base_index);
						const int right_pixel_val_diff_from_avg = r_img.at(win_disp_index) - r_mean_img.at(base_disp_index);
						upper_sum += left_pixel_val_diff_from_avg * right_pixel_val_diff_from_avg;
						lower_sum_0 += left_pixel_val_diff_from_avg * left_pixel_val_diff_from_avg; 
						lower_sum_1 += right_pixel_val_diff_from_avg * right_pixel_val_diff_from_avg;
					}
				}
			}

			float zncc_value = (float) upper_sum / (float)(sqrt(lower_sum_0) * sqrt(lower_sum_1));
			
			if (zncc_value > l_disp_map_max) // for scaling
			{
				l_disp_map_max = zncc_value;
			}

			l_disp_map.push_back(zncc_value);
			
		}
		
	}

	float scaleFactor = (1 / l_disp_map_max) * 255; 

	for (int i = 0; i < l_disp_map.size(); i++)
	{
		l_disp_img.push_back((unsigned char) (l_disp_map.at(i) * scaleFactor)); // use same scale factor for l and r scaling?
	}

	// TODO remove this test fast DEBUG only
	std::string filename = "output/testprintvector.png";
	encodeGreyImg(filename, l_disp_img, img_width, img_height);

}

// Method is based on example_decode.cpp from lodepng examples https://raw.githubusercontent.com/lvandeve/lodepng/master/examples/example_decode.cpp
bool decodeImg(const std::string &filename, std::vector<unsigned char> &image, unsigned &width, unsigned &height)
{
	unsigned int error = lodepng::decode(image, width, height, filename);
	//if there's an error, display it - else show size of image
	
	if (error)
	{
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
		return false;
	}

	return true;
	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...
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
	std::string l_file;
	std::string r_file;
	std::string grey_l_file, grey_r_file, mean_l_file, mean_r_file;
	unsigned img_width, img_height;
	unsigned win_size, max_disparity;
	unsigned sampling_step;

	unsigned l_width, l_height, r_width, r_height; // to check that images have same width and height
	bool write_grey_scale_img_to_file;
	bool write_mean_value_img_to_file;
	bool writeProgress; 


	// hard coded values - TODO: argument reader and real values
	write_grey_scale_img_to_file = false;
	write_mean_value_img_to_file = true;
	sampling_step = 4;
	writeProgress = true;
	grey_l_file = "output/grey_im0.png";
	grey_r_file = "output/grey_im1.png";
	mean_l_file = "output/mean_im0.png";
	mean_r_file = "output/mean_im1.png";
	win_size = 9; // use bigger value - should be 9 ?
	max_disparity = 65;  // use bigger value, only for early-testing TODO  - should be 65?
	

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

	if (!(decodeImg(l_file, l_img, l_width, l_height)
		&& decodeImg(r_file, r_img, r_width, r_height)))
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

	// resize and grayscale images one by one
	resizeAndGreyScaleImg(l_img, l_width, l_height, sampling_step);
	resizeAndGreyScaleImg(r_img, r_width, r_height, sampling_step);

	// images are same sized - use img_height as common name
	img_height = l_height;
	img_width = l_width;

	if (write_grey_scale_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding grey images.. " << std::endl;
		}

		encodeGreyImg(grey_l_file, l_img, img_width, img_height);
		encodeGreyImg(grey_r_file, r_img, img_width, img_height);
	}

	if (writeProgress)
	{
		std::cout << "Creating mean calculations.. " << getCounter(PCFreq, CounterStart) << std::endl;
	}


	// calculate meanIMgs
	createMeanImg(l_img, l_mean_img, l_width, l_height, win_size);
	createMeanImg(r_img, r_mean_img, r_width, r_height, win_size);



	if (write_mean_value_img_to_file)
	{
		if (writeProgress)
		{
			std::cout << "Encoding mean calculations to image.. " << getCounter(PCFreq, CounterStart) << std::endl;
		}


		encodeGreyImg(mean_l_file, l_mean_img, img_width, img_height);
		encodeGreyImg(mean_r_file, r_mean_img, img_width, img_height);
	}

	/*
	// check that we got something  TODO: remove
	std::cout << (unsigned int)l_img.at(100) << std::endl;
	std::cout << (unsigned int)r_img.at(100) << std::endl;

	*/

	if (writeProgress)
	{
		std::cout << "Starting zncc..." << getCounter(PCFreq, CounterStart) << std::endl;
	}

	// TODO to implement functionality
	calc_zncc(l_img, r_img, l_mean_img, r_mean_img, img_width, img_height, win_size, max_disparity, writeProgress);

	if (writeProgress)
	{
		std::cout << "Todo > post process." << getCounter(PCFreq, CounterStart) << std::endl;
	}

	system("pause");
    return 0;
}

