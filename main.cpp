#include "jpeg-to-web.h"
#include "include/tools.h"

int main_01()
{
    VideoCapture cap;
    cap.open("/home/dylan/dy_ws/data/2.mp4");
    if ( !cap.isOpened() )
    {
        printf("no cam found ;(.\n");
        pthread_exit(NULL);
    }

    Mat frame;
    cap >> frame;

    JPEGTOWEB test(7777);
    test.write(frame);
    frame.release();
    test.start();

    while( cap.isOpened() )
    {
        cap >> frame;
        test.write(frame);
        frame.release();
    }

    test.stop();

    return 0;
}

int main()
{
    string filePath = "/home/dylan/dy_ws/code/DirtyDetection-deploy/data/daytime";

    vector<string> imgPath;
    HelpToos::listFiles(filePath.c_str(), ".jpg", imgPath);
    std::sort(imgPath.begin(), imgPath.end());

    Mat frame = cv::imread(filePath + "/" + imgPath[0]);
    int testCount = imgPath.size();

    JPEGTOWEB test(7777);
    test.write(frame);
    frame.release();
    test.start();

    for( int i = 1; i < testCount; ++i )
    {
        frame = cv::imread(filePath + "/" + imgPath[i]);
        if (frame.empty())
            continue;

        test.write(frame);
        frame.release();
        waitKey(30);
    }

    test.stop();

    return 0;
}
