//============================================================================
// Name        : newfloorplanner.cpp
// Author      : zakarya
// Version     :
// Copyright   :
// Description : Layout generator RAW paper
//============================================================================
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <math.h>
#include <map>
#include <bits/stdc++.h>
#include <chrono>  // for high_resolution_clock

#ifdef GRAPHIC_USE
#include<GL/glut.h> 
#include "lib/bitmap_image.hpp"
#endif




using namespace std;
/*
 Outline: <width> <height>

 <blockname> <area>
 <blockname> <area>
 <blockname> <x> <y> <block width> <block height>    <-- this is a preplaced block identifying FPGA areas that should not be used by the floorplanner

 */

typedef uint32_t pint;
typedef uint64_t xint;
typedef unsigned int uint;

int is_prime(xint);

inline int next_prime(pint p) {
    if (p == 2)
        return 3;
    for (p += 2; p > 1 && !is_prime(p); p += 2)
        ;
    if (p == 1)
        return 0;
    return p;
}

int is_prime(xint n) {
#  define NCACHE 256
#  define S (sizeof(uint) * 2)
    static uint cache[NCACHE] = { 0 };

    pint p = 2;
    int ofs, bit = -1;

    if (n < NCACHE * S) {
        ofs = n / S;
        bit = 1 << ((n & (S - 1)) >> 1);
        if (cache[ofs] & bit)
            return 1;
    }

    do {
        if (n % p == 0)
            return 0;
        if (p * p > n)
            break;
    } while ((p = next_prime(p)));

    if (bit != -1)
        cache[ofs] |= bit;
    return 1;
}

int isPrime(int n)
{
  int i;
  int isPrime = 1;
  
  for(i = 2; i <= n / 2; ++i)
  {
      if(n % i == 0)
      {
          isPrime = 0;
          break;
      }
  }
  
  return isPrime;
}


int decompose(xint n, pint *out) {
    int i = 0;
    pint p = 2;
    while (n >= p * p) {
        while (n % p == 0) {
            out[i++] = p;
            n /= p;
        }
        if (!(p = next_prime(p)))
            break;
    }
    if (n > 1)
        out[i++] = n;
    return i;
}

int startswidth(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

class block {

public:

    int area = 0, x = 0, y = 0, width = 0, height = 0;
    int preplaced = 0;
    int rigid = 0;
    char *name = NULL;
    pint *primes = NULL;
    pint *exponents = NULL;
    pint *tmp_exponents = NULL;
    int primes_len = 0;

    block() {
    }

    /**
     * initialization for a bin
     */
    block(int x, int y, int width, int height, int area) {
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
        this->area = area;
    }

    ~block() {
        if (name)
            free(name);
        if (primes)
            free(primes);
        if (exponents)
            free(exponents);
        if (tmp_exponents)
            free(tmp_exponents);
    }

    void prime_factor_decompose_area() {

        // release old information
        if (primes)
            free(primes);
        if (exponents)
            free(exponents);
        if (tmp_exponents)
            free(tmp_exponents);
        primes = exponents = tmp_exponents = NULL;
        primes_len = 0;

        // return immediately, if non-soft module
        if (preplaced || rigid)
            return;

        // soft module, decompose area
        pint out[100];
        primes_len = decompose(area, out);
        pint lprimes[100];
        pint lexponents[100];

        lprimes[0] = out[0];
        lexponents[0] = 1;
        int j = 0;
        for (int i = 1; i < primes_len; i++)
            if (lprimes[j] == out[i])
                lexponents[j]++;
            else {
                lprimes[++j] = out[i];
                lexponents[j] = 1;
            }
        primes_len = j + 1;

        primes = (pint*) malloc(primes_len * sizeof(pint));
        memcpy(primes, lprimes, primes_len * sizeof(pint));

        exponents = (pint*) malloc(primes_len * sizeof(pint));
        memcpy(exponents, lexponents, primes_len * sizeof(pint));

        tmp_exponents = (pint*) malloc(primes_len * sizeof(pint));
    }

    void print() {
        printf("name: %s area: %d x: %d y: %d width: %d height: %d pre-placed: %d\nprime factor decomposition: ", name, area, x, y, width, height, preplaced);
        for (int i = 0; i < primes_len; i++)
            printf("%u^%u ", primes[i], exponents[i]);
        printf("\nprime factor decomposition[2]: ");
        for (int i = 0; i < primes_len; i++)
            printf("%u^%u ", primes[i], tmp_exponents[i]);
        printf("\n");
    }

    /**
     * compute the width and height of the tmp_exponents configuration
     * return the width
     */
    int compute_width_height() {
        if (!preplaced) {
            width = 1;
            for (int i = 0; i < primes_len; i++)
                width *= (int) pow(primes[i], tmp_exponents[i]);
            height = area / width;
        }
        return width;
    }

    void print_block_configurations() {
        compute_width_height();
        printf("%s w=%d h=%d ", name, width, height);
    }
};

class fpga_floorplan {

public:

    int fpga_width;
    int fpga_height;
    vector<block*> *block_list;

    fpga_floorplan() {
        fpga_width = fpga_height = 0;
        block_list = new vector<block*>();
    }

    ~fpga_floorplan() {
        for (auto &v : *block_list)
            free(v);
        free(block_list);
    }

    void load(std::string filename) {

        FILE *f;
        if (filename.empty())
            f = stdin;
        else if (!(f = fopen(filename.c_str(), "r"))) {
            printf("can't open %s\n", filename);
            exit(1);
        }
        char buffer1[25501];
        char *line = buffer1;
        char *ptr;

        while (fgets(line, 25500, f)) {
            if (startswidth(line, "#")) {
                continue;
            } else if (strlen(line) <= 1)
                continue;
            else if (startswidth(line, "Outline:")) {
                ptr = strtok(line, " ");
                ptr = strtok(NULL, " ");
                fpga_width = atoi(ptr);
                ptr = strtok(NULL, " ");
                fpga_height = atoi(ptr);
            } else {    // read block name and 1 to 4 integers

                block *b = new block();

                // copy block name; malloc space for it
                char *name = strtok(line, " ");
                int name_size = strlen(name);
                b->name = (char*) malloc((name_size + 1) * sizeof(char));
                strcpy(b->name, name);

                // continue with area
                ptr = strtok(NULL, " ");
                b->area = atoi(ptr);

                // regular module with area specified only?
                if ((ptr = strtok(NULL, " "))) {
                    // probably rigid module
                    b->rigid = 1;
                    b->width = b->area;
                    b->height = atoi(ptr);
                    if ((ptr = strtok(NULL, " "))) {
                        // pre-placed module
                        b->preplaced = 1;
                        b->x = b->width;
                        b->y = b->height;
                        b->width = atoi(ptr);
                        ptr = strtok(NULL, " ");
                        b->height = atoi(ptr);
                    }
                    b->area = b->width * b->height;

                }
                block_list->push_back(b);

            }
        } // end while
        fclose(f);
    }

    /****************************************************************************************************
     *
     * Here are coming the functions for FPGA map manipulation and floorplanning
     *
     ****************************************************************************************************/
    /*vector<block*>* createframe(vector<block*> *block_list){
        
        vector<block*> *frame = new vector<block*>();
        int size_frame=0;
        for (auto &v : *block_list){
            if (size_frame+v->width<=fpga_width){
                size_frame=size_frame+v->width;
                frame->push_back(v);
            }
        }     
    
        return frame;
    }*/

    void create_frame(vector<block*> &block_list,vector<block*> &frame){
        
        int size_frame=0;
        for(int i = 1; i <= frame.size(); i++) {
            size_frame=size_frame+frame.at(i - 1)->width;
        }    
        for (int i=1;i<=block_list.size(); i++){
            if (size_frame+block_list.at(i - 1)->width<=fpga_width){
                size_frame=size_frame+block_list.at(i - 1)->width;
                frame.push_back(block_list.at(i - 1));
                block_list.erase(block_list.begin()+i-1);
            }
        }     
    }


    void place() {
    
    //First Step:

    
    vector<block*> *groups = new vector<block*>[10];

    for (int i = 1; i <= block_list->size(); i++) {
        if(block_list->at(i - 1)->area>=fpga_width & isPrime(block_list->at(i - 1)->area)==1)
            block_list->at(i - 1)->area=block_list->at(i - 1)->area+1;
        if (block_list->at(i - 1)->area<=fpga_width){
            block_list->at(i - 1)->width=block_list->at(i - 1)->area;
            block_list->at(i - 1)->height=1;
            groups[block_list->at(i - 1)->height-1].push_back(block_list->at(i - 1));
        }else{
            for(int j=2; j<=10;j++){
                if(block_list->at(i - 1)->area % j==0){
                    block_list->at(i - 1)->width=block_list->at(i - 1)->area/j;
                    block_list->at(i - 1)->height=j;
                    groups[j-1].push_back(block_list->at(i - 1));
                    break;
                }
            }
        }
    }

    int j=0;
    while(groups[j].size()!=0){
        // cout<<"G"<<j+1<< endl;
        for(int i = 1; i <= groups[j].size(); i++) {   
            cout<<groups[j].at(i - 1)->name<<" ";   
        }
        cout<<endl;
        j++;
    }
    
    

    //Second Step & Thirs Step:


    vector<vector<block*>> frames(50, vector<block*>());

    vector<int> size(50, 0);
    if (j < frames.size()) {
    // Access element of the frames vector
    cout << "frames size: " << frames.size() << endl;

    }
    cout << "j before decrement: " << j << endl;
    j--;

    int nbrGroups=j;
    int k=0;
    int nbrBlks=block_list->size();
    cout<<"block_list_size"<<block_list->size()<<endl;
    int yy=0;
    // while(nbrBlks>0){
    //     cout<<"Loop "<<nbrBlks<<endl;
    //     j=nbrGroups;
    //     while(j>=0){
    //         cout<<"j inside while "<<j<<endl;
    //         for(int i = 1; i <= groups[j].size(); i++){
    //             if (size[k]+groups[j].at(i - 1)->width<=fpga_width){
    //                 groups[j].at(i - 1)->x=size[k];
    //                 groups[j].at(i - 1)->y=yy;
    //                 size[k]=size[k]+groups[j].at(i - 1)->width;
    //                 frames[k].push_back(groups[j].at(i - 1));
    //                 groups[j].erase(groups[j].begin()+i-1);
    //                 nbrBlks--;
    //                 cout<<"Inside while nbrBlks:"<<nbrBlks<<endl;
    //             }
    //         }
    //         j--;
    //     }
    //     yy=yy+frames[k].at(0)->height;
    //     k++;
    //     cout<<"done with while"<<endl;
    //     j=nbrGroups; // Set j to nbrGroups before starting the next iteration
    // }


    while(nbrBlks>0){
         cout<<"Loop "<<nbrBlks<<endl;
        j=nbrGroups;
        while(j>=0){
             cout<<"j inside while"<<j<<endl;
            for(int i = 1; i <= groups[j].size(); i++){
                if (size[k]+groups[j].at(i - 1)->width<=fpga_width){
                    groups[j].at(i - 1)->x=size[k];
                    groups[j].at(i - 1)->y=yy;
                    size[k]=size[k]+groups[j].at(i - 1)->width;
                    frames[k].push_back(groups[j].at(i - 1));
                    groups[j].erase(groups[j].begin()+i-1);
                    nbrBlks--;
                    cout<<"Inside while nbrBlks:"<<nbrBlks<<endl;
                    }
            }
        j--;
      }
      yy=yy+frames[k].at(0)->height;
      k++;
      cout<<"done with while"<<endl;
    }
         

    k--;
    while (k>=0){
        cout<<"F inside another k"<<k<<": ";
    for(int i = 1; i <= frames[k].size(); i++) {   
        cout<<frames[k].at(i - 1)->name<<" ";   
    }
    cout<<endl;
    k--;    
    }
    



    cout<<endl;
    }

    
    

    

    /****************************************************************************************************
     *
     * End of: Here are coming the functions for FPGA map manipulation and floorplanning
     *
     ****************************************************************************************************/

    void print() {
        printf("Outline/FPGA width: %d height: %d\n", fpga_width, fpga_height);
        for (auto const &v : *block_list) {
            v->print();
            printf("\n");
        }
    }

    void print_block_configurations() {
        for (auto const &v : *block_list)
            v->print_block_configurations();
        printf("\n");
    }

};

static vector<block*> *block_list1;
static int W, H;

#ifdef GRAPHIC_USE
// function to initialize
void myInit(void) {
    // making background color black as first
    // 3 arguments all are 0.0
    glClearColor(1, 1, 1, 1.0);

    // making picture color green (in RGB mode), as middle argument is 1.0
    glColor3f(0.0, 1.0, 0.0);

    // breadth of picture boundary is 1 pixel
    glPointSize(1.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // setting window dimension in X- and Y- direction
    gluOrtho2D(-5, W + 5, -5, H + 5);

}

void drawline(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void drawgrid() {
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glColor3ub(0, 0, 0);

    for (float i = 0; i < H; i += 1) {
        glLineWidth(1);
        drawline(0, i, (float) W, i);
    }

    for (float i = 0; i < W; i += 1) {
        glLineWidth(1);
        drawline(i, 0, i, (float) H);
    }
}

void displayfunc(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glColor3f(0, 0, 0);
    glVertex2i(0, 0);
    glVertex2i(0 + W, 0);
    glVertex2i(0 + W, 0 + H);
    glVertex2i(0, 0 + H);
    glEnd();
    glBegin(GL_QUADS);
    glColor3f(0.6, 0.6, 0.6);
    glVertex2i(0, 0);
    glVertex2i(0 + W, 0);
    glVertex2i(0 + W, 0 + H);
    glVertex2i(0, 0 + H);
    glEnd();
    drawgrid();
    for (int i = 1; i <= block_list1->size(); i++) {
        glLineWidth(3);
        glBegin(GL_LINE_LOOP);
        glColor3f(0, 0, 0);
        glVertex2i(block_list1->at(i - 1)->x, block_list1->at(i - 1)->y);
        glVertex2i(block_list1->at(i - 1)->x + block_list1->at(i - 1)->width, block_list1->at(i - 1)->y);
        glVertex2i(block_list1->at(i - 1)->x + block_list1->at(i - 1)->width, block_list1->at(i - 1)->y + block_list1->at(i - 1)->height);
        glVertex2i(block_list1->at(i - 1)->x, block_list1->at(i - 1)->y + block_list1->at(i - 1)->height);
        glEnd();
    }
    unsigned char *imageData = (unsigned char*) malloc((int) (500 * 500 * (3)));
    glReadPixels(0, 0, 500, 500, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    //write
    bitmap_image image(500, 500);
    image_drawer draw(image);
    for (unsigned int i = image.height() - 1; i > 0; --i) {
        for (unsigned int j = 0; j < image.width(); ++j) {
            image.set_pixel(j, i, *(++imageData), *(++imageData), *(++imageData));
        }
    }

    image.save_image("floorplan_1.bmp");
    glFlush();
}
#endif

void resultsFile() {
    //ofstream file;
    //file.open("results");
    cout<<"--------------------------------"<<endl;
    cout << "Outline: " << H << " " << W << endl;
    for (int i = 1; i <= block_list1->size(); i++) {
        cout << block_list1->at(i - 1)->name << " ";
        cout << block_list1->at(i - 1)->area << "\t: ";
        cout << block_list1->at(i - 1)->x << " ";
        cout << block_list1->at(i - 1)->y << " / ";
        cout << block_list1->at(i - 1)->width << " ";
        cout << block_list1->at(i - 1)->height << endl;
    }
    //file.close();
}

int computeBoundingArea() {
    int min_x = INT_MAX, min_y = INT_MAX;
    int max_x = INT_MIN, max_y = INT_MIN;
    for (int i = 0; i < block_list1->size(); i++) {
        block* b = block_list1->at(i);
        min_x = min(min_x, b->x);
        min_y = min(min_y, b->y);
        max_x = max(max_x, b->x + b->width);
        max_y = max(max_y, b->y + b->height);
    }
    // cout<< "min_x " << min_x << endl;
    // cout<< "min_y " << min_y << endl;
    // cout<< "max_x " << max_x << endl;
    // cout<< "max_y " << max_y << endl;
    int bounding_rect_width = max_x - min_x;
    int bounding_rect_height = max_y - min_y;
    int total_bounding_area = bounding_rect_width * bounding_rect_height;
    cout << "Total_bounding_area " << total_bounding_area << endl;
    return total_bounding_area;
}


// void processFile(const string& filename, ofstream& csvFile)
// {
//     fpga_floorplan *fp = new fpga_floorplan();

//     fp->load(filename.c_str());

//     printf("loading done\n");

//     printf("placing...\n");
//     auto start_time = std::chrono::steady_clock::now(); // Start measuring time

//     fp->place();

//     auto end_time = std::chrono::steady_clock::now(); // Stop measuring time

//     if (fp->block_list.empty()) {
//         cout << "Skipping file " << filename << endl;
//         return;
//     }

//     block_list1 = fp->block_list;
//     H = fp->fpga_height;
//     W = fp->fpga_width;
//     int boundingArea = computeBoundingArea();

//     // Extract the file name without extension
//     string basename = filename.substr(filename.find_last_of("/\\") + 1);
//     //basename = basename.substr(0, basename.find_last_of('.'));

//     // Output results to console
//     cout << "Bounding area for " << basename << " is " << boundingArea << endl;

//     // Write results to CSV file
//     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
//     csvFile << basename << "," << boundingArea << "," << duration.count() << endl;
// }

void processFile(const string& filename, ofstream& csvFile)
{
    fpga_floorplan *fp = new fpga_floorplan();

    try {
        fp->load(filename.c_str());

        printf("loading done\n");

        printf("placing...\n");
        auto start_time = std::chrono::steady_clock::now(); // Start measuring time

        fp->place();

        auto end_time = std::chrono::steady_clock::now(); // Stop measuring time

        block_list1 = fp->block_list;
        H = fp->fpga_height;
        W = fp->fpga_width;
        int boundingArea = computeBoundingArea();

        // Extract the file name without extension
        string basename = filename.substr(filename.find_last_of("/\\") + 1);
        //basename = basename.substr(0, basename.find_last_of('.'));

        // Output results to console
        cout << "Bounding area for " << basename << " is " << boundingArea << endl;

        // Write results to CSV file
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        csvFile << basename << "," << boundingArea << "," << duration.count() << endl;
    }
    catch (const std::exception& e) {
        cout << "Skipping file " << filename << ": " << e.what() << endl;
    }

    delete fp;
}



// int main()
// {
//     ofstream csvFile("final_heuristic_LBMA_results.csv");
//     csvFile << "filename,final_bounding_area,runtime" << endl;
    
//     for (int i = 0; i < 60; i++)
//     {
//         string filename = "LBMA_Data/LBMA_" + to_string(i) + ".txt";
//         processFile(filename, csvFile);
//     }

//     csvFile.close();

//     return 0;
// }


int main(int argcc, char **argv) {

    fpga_floorplan *fp = new fpga_floorplan();

    if (argcc == 1)
        fp->load(NULL);
    else
        fp->load(argv[1]);

    printf("loading done\n");

    printf("placing...\n");
    fp->place();

    block_list1 = fp->block_list;
    H = fp->fpga_height;
    W = fp->fpga_width;
    resultsFile();
    computeBoundingArea();

#ifdef GRAPHIC_USE
    glutInit(&argcc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    giving window size in X- and Y- direction
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(0, 0);

    Giving name to window
    glutCreateWindow("floorplanner");
    myInit();

    glutDisplayFunc(displayfunc);
    glutMainLoop();
#endif


    return (0);
} // END usage