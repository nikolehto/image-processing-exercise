// zncc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


// Method is based on example_decode.cpp from lodepng examples https://raw.githubusercontent.com/lvandeve/lodepng/master/examples/example_decode.cpp
void decodeOneStep(const std::string &filename, std::vector<unsigned char> &image)
{
	
	unsigned width, height;

	//decode
	unsigned error = lodepng::decode(image, width, height, filename);

	//if there's an error, display it - else show size of image
	if (error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
	else std::cout << "size: " << width << "*" << height << std::endl;
	//the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...

	// check
	//std::cout << (unsigned int)image.at(100) << std::endl;
}


int main(int argc, char *argv[])
{
	std::vector<unsigned char> l_img; 
	std::vector<unsigned char> r_img; 
	std::string l_file;
	std::string r_file;

	//std::cout << l_img.at(100) << std::endl;

	if (argc == 2)
	{
		l_file = *argv[1];
		r_file = *argv[2];
	}
	else
	{ 
		l_file = "images/im0.png";
		r_file = "images/im0.png";
	}
	decodeOneStep(l_file, l_img);
	decodeOneStep(r_file, r_img);

	// check
	//std::cout << (unsigned int) l_img.at(100) << std::endl;
	//std::cout << (unsigned int) r_img.at(100) << std::endl;

	system("pause");
    return 0;
}

