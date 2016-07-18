#include <ros/ros.h>

// opencv image processing libraries
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/objdetect/objdetect.hpp"

// to facilitate the processing of the images in ROS
#include <image_transport/image_transport.h> // for publishing and subscribing to images in ROS
#include <cv_bridge/cv_bridge.h> // to convert between ROS and OpenCV Image formats
#include <sensor_msgs/image_encodings.h>

#include <hog_haar_person_detection/Faces.h>
#include <hog_haar_person_detection/Face.h>

static const std::string OPENCV_WINDOW = "Image window";

class HogHaarPersonDetection
{
public:
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber im_sub_;
  image_transport::Publisher im_pub_;

  ros::Publisher faces_pub_;

  cv::CascadeClassifier face_cascade_;

  cv::HOGDescriptor hog_;

  hog_haar_person_detection::Faces facesMessage;

  HogHaarPersonDetection()
    :  it_(nh_)
  {
    // Get ROS params from launch file
    std::string image_topic;
    if (!nh_.getParam("image_topic", image_topic))
      ROS_ERROR("Could not get image_topic");

    std::string face_cascade_name_std;
    if (!nh_.getParam("face_cascade_name", face_cascade_name_std))
      ROS_ERROR("Could not get face_cascade_name");
    cv::String face_cascade_name = face_cascade_name_std;


    // Load the hog descriptor
    hog_.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

    // Load face detector HAAR cascades
    if(!face_cascade_.load(face_cascade_name))
      ROS_ERROR("--(!)Error loading face detector cascade \n");

    // Subscrive to input video feed and publish output video feed
    im_sub_ = it_.subscribe(image_topic, 1, &HogHaarPersonDetection::imageCallback, this);
    im_pub_ = it_.advertise("/camera_person_tracker/output_video", 1);
    faces_pub_ = nh_.advertise<hog_haar_person_detection::Faces>("/person_detection/faces", 1000);

    cv::namedWindow(OPENCV_WINDOW);
  }


  ~HogHaarPersonDetection()
  {
    cv::destroyWindow(OPENCV_WINDOW);
  }

  void addFace(int pos_x, int pos_y, int size_x, int size_y, int score = 0){
    hog_haar_person_detection::Face face;
    face.center_pos.x = pos_x;
    face.center_pos.y = pos_y;
    face.size_x = size_x;
    face.size_y = size_y;
    face.score = score;

    facesMessage.faces.push_back(face);
  }

  void resetFaces(){
    facesMessage.faces.clear();
  }

  void publishFaces(){
    faces_pub_.publish(facesMessage);
  }

  void imageCallback(const sensor_msgs::ImageConstPtr& msg)
  {
    // Make image processable in ROS
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }
    cv::Mat im_bgr = cv_ptr->image;

    // HAAR Face detector
    std::vector<cv::Rect> detected_faces;
    cv::Mat im_gray;
    cv::cvtColor( im_bgr, im_gray, CV_BGR2GRAY );
    cv::equalizeHist( im_gray, im_gray );
    face_cascade_.detectMultiScale( im_gray, detected_faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE, cv::Size(30, 30) );

    resetFaces();

    // Draw detected detected_faces to screen as circles
    for(unsigned i = 0; i < detected_faces.size(); i++)
    {
      cv::Point center( detected_faces[i].x + detected_faces[i].width*0.5, detected_faces[i].y + detected_faces[i].height*0.5 );
      cv::ellipse( im_bgr, center, cv::Size( detected_faces[i].width*0.5, detected_faces[i].height*0.5), 0, 0, 360, cv::Scalar( 255, 0, 255 ), 4, 8, 0 );

      addFace(detected_faces[i].x, detected_faces[i].y, detected_faces[i].width, detected_faces[i].height, 0);
    }

    publishFaces();

    // HOG pedestrian detector
    std::vector<cv::Rect> detected_pedestrian;
    hog_.detectMultiScale(im_gray, detected_pedestrian, 0.0, cv::Size(8, 8), cv::Size(0,0), 1.05, 4);

    // Draw detections from HOG to the screen
    for(unsigned i = 0; i < detected_pedestrian.size(); i++)
    {
      cv::rectangle(im_bgr, detected_pedestrian[i], cv::Scalar(255));
    }
  }
};


int main (int argc, char** argv)
{
  ros::init (argc, argv, "hog_haar_person_detection");
  HogHaarPersonDetection hhpd;
  ros::spin ();
  return 0;
}
