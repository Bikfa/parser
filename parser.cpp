#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <fstream>
#include <opencv2/opencv.hpp>

/*
#######################################
#   CRYSYS IMAGE FILE FORMAT (CIFF)   #
#######################################
 */

struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct CiffHeader {
    std::string magic; //4 ASCII character spelling 'CIFF'
    uint64_t header_size; //size of the header (all fields included)
    uint64_t content_size; //size of the image pixels, width*heigth*3
    uint64_t width;
    uint64_t height;
    std::string caption;
    std::vector<std::string> tags; //each separated by \0 characters
};

struct CiffContent {
    std::vector<Pixel> pixels;
};

struct CiffImage {
    CiffHeader ciffheader;
    CiffContent ciffcontent;
};

/*
###########################################
#   CRYSYS ANIMATION FILE FORMAT (CAFF)   #
###########################################
 */

struct CaffHeader {
    std::string magic; //4 ASCII character spelling 'CAFF'
    uint64_t header_size; //size of the header (all fields included)
    uint64_t num_anim; //number of CIFF animation blocks in the CAFF file
};

struct CaffCredits {
    uint16_t year;
    uint8_t month, day, hour, minute;
    uint64_t creator_len; //length of the field specifying the creator
    std::string creator; //creator of the CAFF file
};

struct CaffAnimation {
    uint16_t duration; //miliseconds for which the CIFF image must be displayed during animation
    CiffImage ciff;
};

//https://en.cppreference.com/w/cpp/utility/variant
//represents a type-safe union
//holds a value of one of its alternative types,
using CaffData = std::variant<CaffHeader, CaffCredits, CaffAnimation>;

struct CaffBlock {
    uint8_t id; //identifies the type of the block, 0x1 - header, 0x2 - credits, 0x3 - animation
    uint64_t length;
    //contain the data of the block
    CaffData data;
};

struct CaffFile {
    std::vector<CaffBlock> blocks;
};

CiffImage CIFFfiletoJPEG;
/*
#################
#   File check  #
#################
 */
CiffImage getCIFF(std::ifstream& file, std::streampos startPos) {
    CiffImage ciffimage;

    std::cout << "CIFF start" << std::endl;

    std::string magic(4, '\0');
    file.read(&magic[0], 4);
    ciffimage.ciffheader.magic = magic;
    std::cout << "CIFF header magic: " << ciffimage.ciffheader.magic << std::endl;
    if (ciffimage.ciffheader.magic != "CIFF") {
        std::cout << ciffimage.ciffheader.magic << std::endl;
        return ciffimage;
    }

    //reads the specified number of bytes from the file stream and assigns them to the into the specified variable
    file.read(reinterpret_cast<char*>(&ciffimage.ciffheader.header_size), sizeof(ciffimage.ciffheader.header_size));
    std::cout << "Header size: " << static_cast<int>(ciffimage.ciffheader.header_size) << std::endl;
    file.read(reinterpret_cast<char*>(&ciffimage.ciffheader.content_size), sizeof(ciffimage.ciffheader.content_size));
    file.read(reinterpret_cast<char*>(&ciffimage.ciffheader.width), sizeof(ciffimage.ciffheader.width));
    file.read(reinterpret_cast<char*>(&ciffimage.ciffheader.height), sizeof(ciffimage.ciffheader.height));
    std::getline(file, ciffimage.ciffheader.caption);
    std::string tag;
    while (std::getline(file, tag, '\0')) {
        ciffimage.ciffheader.tags.push_back(tag);
    }

    for (uint64_t i = 0; i < ciffimage.ciffheader.width * ciffimage.ciffheader.height; ++i) {
        Pixel pixel;
        file.read(reinterpret_cast<char*>(&pixel.red), sizeof(pixel.red));
        file.read(reinterpret_cast<char*>(&pixel.green), sizeof(pixel.green));
        file.read(reinterpret_cast<char*>(&pixel.blue), sizeof(pixel.blue));
        ciffimage.ciffcontent.pixels.push_back(pixel);
    }
    return ciffimage;
}

bool isCIFF(std::ifstream& file, std::streampos startPos) {
    CiffImage ciffimage = getCIFF(file, startPos);

    if (ciffimage.ciffheader.magic != "CIFF") {
        return false;
    }

    if (ciffimage.ciffheader.content_size != ciffimage.ciffheader.width * ciffimage.ciffheader.height * 3) {
        return false;
    }

    if (startPos != 0) {
        return file.eof(); //checking for unexpected end of file
    }
    else {
        CIFFfiletoJPEG = ciffimage;
        return true;
    }
}

bool isCAFF(std::ifstream& file) {
    CaffFile cafffile;

    bool firstanimationblock = true;

    // Read ID and length of the block
    uint8_t id;
    uint64_t length;
    file.read(reinterpret_cast<char*>(&id), sizeof(id));
    file.read(reinterpret_cast<char*>(&length), sizeof(length));

    // Ensure the first block is a header
    if (id != 0x1) {
        return false;
    }

    //The first block of all CAFF files is the CAFF header
    CaffHeader firstcaffheader;

    std::string magic(4, '\0');
    file.read(&magic[0], 4);
    firstcaffheader.magic = magic;
    if (firstcaffheader.magic != "CAFF") {
        return false;
    }

    file.read(reinterpret_cast<char*>(&firstcaffheader.header_size), sizeof(firstcaffheader.header_size));
    file.read(reinterpret_cast<char*>(&firstcaffheader.num_anim), sizeof(firstcaffheader.num_anim));

    std::cout << firstcaffheader.num_anim << std::endl;
    //iterating through blocks, based on num_anim
    for (uint16_t i = 0; i < firstcaffheader.num_anim; ++i) {
        CaffBlock block;
        file.read(reinterpret_cast<char*>(&block.id), sizeof(block.id));
        file.read(reinterpret_cast<char*>(&block.length), sizeof(block.length));
        std::cout  << static_cast<int>(block.id) << std::endl;;

        if (block.id == 0x1) { //header
            CaffHeader caffheader;
            file.read(reinterpret_cast<char*>(&caffheader.header_size), sizeof(caffheader.header_size));
            file.read(reinterpret_cast<char*>(&caffheader.num_anim), sizeof(caffheader.num_anim));
            block.data = caffheader;
        }
        else if (block.id == 0x2) { //credits
            CaffCredits caffcredits;
            file.read(reinterpret_cast<char*>(&caffcredits.year), sizeof(caffcredits.year));
            file.read(reinterpret_cast<char*>(&caffcredits.month), sizeof(caffcredits.month));
            std::cout << "Year: " << caffcredits.year << std::endl;
            std::cout << "Month: " << static_cast<int>(caffcredits.month) << std::endl;
            if (static_cast<int>(caffcredits.month) < 1 || static_cast<int>(caffcredits.month) > 12) {
                return false;
            }
            file.read(reinterpret_cast<char*>(&caffcredits.day), sizeof(caffcredits.day));
            std::cout << "Day: " << static_cast<int>(caffcredits.day) << std::endl;
            if (static_cast<int>(caffcredits.day) < 1 || static_cast<int>(caffcredits.day) > 31) {
                return false;
            }
            file.read(reinterpret_cast<char*>(&caffcredits.hour), sizeof(caffcredits.hour));
            std::cout << "Hour: " << static_cast<int>(caffcredits.hour) << std::endl;
            if (static_cast<int>(caffcredits.hour) > 23) {
                return false;
            }
            file.read(reinterpret_cast<char*>(&caffcredits.minute), sizeof(caffcredits.minute));
            std::cout << "Minute: " << static_cast<int>(caffcredits.minute) << std::endl;
            if (static_cast<int>(caffcredits.minute) > 59) {
                return false;
            }
            file.read(reinterpret_cast<char*>(&caffcredits.creator_len), sizeof(caffcredits.creator_len));
            std::cout << "Creatir_len: " << static_cast<int>(caffcredits.creator_len) << std::endl;
            std::vector<char> creatorBuffer(caffcredits.creator_len);
            file.read(creatorBuffer.data(), caffcredits.creator_len);
            caffcredits.creator = std::string(creatorBuffer.data(), creatorBuffer.size());
            std::cout << "Creator: " << caffcredits.creator << std::endl;
            block.data = caffcredits;
        }
        else if (block.id == 0x3) { // animation
            if (firstanimationblock == true) {
                std::cout << static_cast<int>(block.id) << std::endl;
            }
            firstanimationblock = false;
            CaffAnimation caffanimation;
            file.read(reinterpret_cast<char*>(&caffanimation.duration), sizeof(caffanimation.duration));
            if (static_cast<int>(caffanimation.duration) < 1) {
                return false;
            }
            std::cout << "Anim duration: " << static_cast<int>(caffanimation.duration) << std::endl;
            std::cout << "Meret: " << sizeof(caffanimation.duration) << std::endl;
            std::string nullm(6, '\0');
            file.read(&nullm[0], 6);
            std::streampos startPos = file.tellg();
            if (isCIFF(file, 1)) {
                std::cout << "Its a CIFF" << std::endl;
                CiffImage ciffimage;
                ciffimage.ciffheader = caffanimation.ciff.ciffheader;
                ciffimage.ciffcontent = caffanimation.ciff.ciffcontent;
                CIFFfiletoJPEG = ciffimage;
                return true;
            }
            else {
                return false;
            }
            std::cout << caffanimation.ciff.ciffheader.magic << std::endl;
            block.data = caffanimation;
        }
        else {
            return false;
        }

        cafffile.blocks.push_back(block);
    }

    return file.eof(); //check for unexpected end of file
}

/*
#####################
#   Convert to jpg  #
#####################
 */
cv::Mat CIFFtoJPG(const CiffImage& ciffimage) {
    cv::Mat image(ciffimage.ciffheader.height, ciffimage.ciffheader.width, CV_8UC3);

    int index = 0;
    for (int y = 0; y < ciffimage.ciffheader.height; ++y) {
        for (int x = 0; x < ciffimage.ciffheader.width; ++x) {
            const Pixel& pixel = ciffimage.ciffcontent.pixels[index++];
            cv::Vec3b& cvPixel = image.at<cv::Vec3b>(y, x);
            cvPixel[0] = pixel.blue;
            cvPixel[1] = pixel.green;
            cvPixel[2] = pixel.red;
        }
    }

    return image;
}


int main(int argc, char* argv[]) {

    if (argc < 3) {
        return -1;
    }

    std::string filetype = argv[1];
    std::string filename = argv[2];

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return -1;
    }

    if (filetype == "-ciff") {
        if (!isCIFF(file, 0)) {
            return -1;
        }
        else {
            // CIFF convert to JPG
            if (!CIFFfiletoJPEG.ciffcontent.pixels.empty()) {
                cv::Mat jpgImage = CIFFtoJPG(CIFFfiletoJPEG);
                std::string outputFilename = filename + ".jpg";
                cv::imwrite(outputFilename, jpgImage);
                return 0;
            }
            else {
                return -1;
            }
        }
    }
    else if (filetype == "-caff") {
        if (isCAFF(file)) {
            // CIFF convert to JPG
            if (!CIFFfiletoJPEG.ciffcontent.pixels.empty()) {
                cv::Mat jpgImage = CIFFtoJPG(CIFFfiletoJPEG);
                std::string outputFilename = filename + ".jpg";
                cv::imwrite(outputFilename, jpgImage);
                return 0;
            }
            else {
                return -1;
            }
        }
        else {
            return -1;
        }
    }
    else {
        return -1;
    }

    file.close();

    return 0;
}
