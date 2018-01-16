// zncc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

void calc_zncc(std::vector<unsigned char> &l_img
	, std::vector<unsigned char> &r_img
	, unsigned &img_width
	, unsigned &img_height
	, unsigned &win_width
	, unsigned &win_height
	, unsigned &max_disparity)
{
	// TODO algorithm 
	// TODO indexing to image ( rgbarbga.. ) 



	std::vector<unsigned char> disparity_image; // TODO - pass as parameter

	unsigned current_maximum_sum = 0;
	unsigned best_disparity_value = 0;

	unsigned zncc_value = 0;

	unsigned step = 20; 	// todo step parameter for faster testing  - NOTE disparity indexing

	for (unsigned int row = 0; row < img_height; row += step)
	{
		for (unsigned int column = 0; column < img_width; column += step)
		{
			for (unsigned int d = 0; d < max_disparity; d++)
			{
				for (unsigned int win_y = 0; win_y < win_height; win_y++)
				{
					for (unsigned int win_x = 0; win_x < win_width; win_x++)
					{
						// calculate the mean value of each window
					}
				}
				for (unsigned int win_y = 0; win_y < win_height; win_y++)
				{
					for (unsigned int win_x = 0; win_x < win_width; win_x++)
					{
						// calculate zncc value of each window
					}
				}
				//
				if (zncc_value > current_maximum_sum)
				{
					current_maximum_sum = zncc_value; // TODO : is this correct? 
					best_disparity_value = zncc_value; // TODO : is this correct? 
				}
			}

			//disparity_image.at(<index>) = best_disparity_value 
		}
		
	}
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
	std::string l_file;
	std::string r_file;
	unsigned img_width, img_height;
	unsigned win_width, win_height, max_disparity;

	unsigned l_width, l_height, r_width, r_height; // to check that images have same width and height

	// hard coded values - TODO: argument reader 
	win_width = 2;
	win_height = 2;
	max_disparity = 2;  // use bigger value, only for early-testing TODO

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

	if (l_width != r_width || l_height != r_height)
	{
		std::cout << "different sized images, exiting" << std::endl;
		system("pause");
		return 2;
	}

	// check that we got something  TODO: remove
	std::cout << (unsigned int)l_img.at(100) << std::endl;
	std::cout << (unsigned int)r_img.at(100) << std::endl;

	// now images are same sized - use img_height variable so on.. 
	img_height = l_height;
	img_width = l_width;

	calc_zncc(l_img, r_img, img_width, img_height, win_width, win_height, max_disparity);



	system("pause");
    return 0;
}

