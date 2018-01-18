// zncc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

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


void calc_zncc(std::vector<unsigned char> &l_img
	, std::vector<unsigned char> &r_img
	, std::vector<unsigned char> &l_mean_img
	, std::vector<unsigned char> &r_mean_img
	, unsigned &img_width
	, unsigned &img_height
	, unsigned &win_size
	, unsigned &max_disparity)
{
	// TODO algorithm 

	std::vector<unsigned char> l_disp_img; // TODO - pass as parameter 
	std::vector<unsigned char> r_disp_img; 

	const int win_rad_x = (win_size - 1) / 2;
	const int win_rad_y = (win_size - 1) / 2;

	for (int img_y = 0; img_y < img_height; img_y++)
	{
		for (int img_x = 0; img_x < img_width; img_x++)
		{
			const int base_index = img_y * img_width + img_x;

			for (int disp_x = 0; disp_x< max_disparity; disp_x++)
			{

				int upper_sum = 0;
				int lower_sum_0 = 0;
				int lower_sum_1 = 0;

				for (int win_y = -win_rad_y; win_y <= win_rad_y; win_y++) // TODO check <=
				{
					if (img_y + win_y < 0 || img_y + win_y >= img_height) // TODO check >=
					{
						continue;
					}

					for (int win_x = -((int)win_rad_x); win_x <= win_rad_x; win_x++)
					{
						if (img_x + win_x + disp_x < 0 || img_x + win_x + disp_x >= img_width) // TODO check >=
						{
							continue;
						}
						const int index = (img_y + win_y) * img_width + img_x + win_x;
						const int disp_index = (img_y + win_y) * img_width + img_x + win_x + disp_x;

						// TODO : which average ( of window or of pixel index ( i think pixels so use 'base_index' ) )  BUT NO because how about  right_pixel_val_diff_from_avg
						// upper_sum += (l_img.at(index) - l_mean_img.at(<whichindex>)) * (r_img.at(disp_index) - r_mean_img.at(<whichindex>))


					}
				}
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


	// hard coded values - TODO: argument reader and real values
	write_grey_scale_img_to_file = false;
	write_mean_value_img_to_file = true;
	sampling_step = 4;
	grey_l_file = "output/grey_im0.png";
	grey_r_file = "output/grey_im1.png";
	mean_l_file = "output/mean_im0.png";
	mean_r_file = "output/mean_im1.png";
	win_size = 4;
	max_disparity = 2;  // use bigger value, only for early-testing TODO


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

	// resize and grayscale images one by one
	resizeAndGreyScaleImg(l_img, l_width, l_height, sampling_step);
	resizeAndGreyScaleImg(r_img, r_width, r_height, sampling_step);

	// images are same sized - use img_height as common name
	img_height = l_height;
	img_width = l_width;

	if (write_grey_scale_img_to_file)
	{
		encodeGreyImg(grey_l_file, l_img, img_width, img_height);
		encodeGreyImg(grey_r_file, r_img, img_width, img_height);
	}

	// calculate meanIMgs
	createMeanImg(l_img, l_mean_img, l_width, l_height, win_size);
	createMeanImg(r_img, r_mean_img, r_width, r_height, win_size);


	if (write_mean_value_img_to_file)
	{
		encodeGreyImg(mean_l_file, l_mean_img, img_width, img_height);
		encodeGreyImg(mean_r_file, r_mean_img, img_width, img_height);
	}

	/*
	// check that we got something  TODO: remove
	std::cout << (unsigned int)l_img.at(100) << std::endl;
	std::cout << (unsigned int)r_img.at(100) << std::endl;

	*/

	// TODO to implement functionality
	calc_zncc(l_img, r_img, l_mean_img, r_mean_img, img_width, img_height, win_size, max_disparity);



	system("pause");
    return 0;
}

