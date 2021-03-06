
#include <ros/ros.h>
#include <cmath>
#include <string>
#include <iostream>
#include <std_msgs/String.h>
#include <visualization_msgs/Marker.h>
#include <obstacle_detector/Obstacles.h>
#include <obstacle_detector/SegmentObstacle.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/PointStamped.h>
#include <ackermann_msgs/AckermannDriveStamped.h>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#define MAX_STEERING 25     // 왼쪽 최대 조향
#define MIN_STEERING -25    // 오른쪽 최대 조향
//using namespace message_filters;
using namespace std;
class StaticAvoidance{
    private:
        // Segments
        obstacle_detector::SegmentObstacle nearest_segment_data;
        obstacle_detector::SegmentObstacle nearest_segment;
        ackermann_msgs::AckermannDriveStamped acker_data;
        geometry_msgs::Point nearest_segment_center_point_;
        geometry_msgs::Point WayPoint_Marker;
        geometry_msgs::Point SegmentVector;
        //test
        // Values
        float DISTANCE = 100;
        float SPEED = 0;
        int INIT = 0;       // curve_check 만약 INIT이 0과 1만 쓰이면 bool 타입의 변수로 바꾸어주고, 변수이름(curve_check)도 알아보기 쉬운것으로 바꾸는게 좋을듯
        int STATUS = 0;
        float angle = 0;
        bool status_flag_ = false; 
        //float fixed_yaw = 0;
        geometry_msgs::Point wayPoint;
        // Const Values
        const float SeekDistance = 3.0;
        const float ANGLE = 15;
        const float GAP = 2.5;
 
        // Obstacles
        obstacle_detector::Obstacles left_obstacle;
        obstacle_detector::Obstacles right_obstacle;
        obstacle_detector::Obstacles cur_obstacle;
    public:    
        // Ros
        ros::NodeHandle nh_;
        ros::Publisher acker_pub;
        ros::Subscriber obstacle_sub_raw;
        ros::Subscriber obstacle_sub_left;
        ros::Subscriber obstacle_sub_right;
        ros::Publisher vis_pub_;
        visualization_msgs::Marker marker;
        // 생성자에 start()함수를 실행시키게 한다. class 이름이 StaticAvoidance 이므로 생성자 또한 이름이 같다.
        StaticAvoidance(){           
            start();
        }
        // acker_data는 클래스의 멤버변수이므로 다른 함수에서 접근하기 위해서 public 함수를 만들어주었다.
        ackermann_msgs::AckermannDriveStamped getAckerData() {
            return acker_data;
        }
            
        void start(){
            ROS_INFO("STATIC AVOIDANCE START");   // ROS_INFO는 C/C++ 의 printf 나 cout과 같은 기능을 한다.
            obstacle_sub_left = nh_.subscribe("raw_obstacles_left", 10, &StaticAvoidance::obstacle_left_cb, this);   // &를 이용하여 참조형 매개변수를 사용, this로 반환
            obstacle_sub_right = nh_.subscribe("raw_obstacles_right", 10, &StaticAvoidance::obstacle_right_cb, this);
            obstacle_sub_raw = nh_.subscribe("raw_obstacles", 10, &StaticAvoidance::obstacle_cb, this); 
            acker_pub = nh_.advertise<ackermann_msgs::AckermannDriveStamped>("/ctrl_cmd", 10); // ackermann 토픽
            vis_pub_ = nh_.advertise<visualization_msgs::Marker>( "visualization_marker", 10 );
        }
        void exect(){       
            if (INIT == 0){             // INIT이 0일때: STATUS = 0,2,3,4  , INIT이 1일때 : STATUS = 1
                acker_data.drive.speed = 1;
                acker_data.drive.steering_angle = 0;
            }
        }
        float distance(const geometry_msgs::Point point){      
            float dist = sqrt(point.x*point.x+point.y*point.y);
            return dist;
        }
        
        float innerProduct(const geometry_msgs::Point point){
            float distA = 1.0;                  
            float distB = distance(point);
            float ip = point.x * 0 + point.y * 1;
            float ip2 = distA * distB;      // distA=1이고, 1*(숫자A) = (숫자A)인데, distA를 곱해서 계산하는 이유가 있나요?
            float CosValue = ip / ip2;  
            float x = acos(CosValue);
            float degree = x*180/M_PI;    // 3.1415는 M_PI로 나타낼 수 있다.
            return degree;
        }
        
        void findNearestSegment(obstacle_detector::Obstacles obstacle){
            //segments에 대한 타입 다시 생각해보기.... SegmentObstacle의 경우 segments는 1차원 배열 , Obstacles의 경우는 first_point와 last_point를 갖고 있는데 어떤 타입이지?
            geometry_msgs::Point center_point;
            
            //범위기반 for문,  i 에는 인덱스가 아닌 배열의 요소에 저장되어 있는 값이 전달된다.
            for (auto i : obstacle.segments){            //auto를 사용하면 segment와 같은 자료형을 C++이 자료형을 추론해서 변수 i에 같은 자료형으로 맞춰줌.
                center_point.x = (i.first_point.x + i.last_point.x)/2;      
                center_point.y = (i.first_point.y + i.last_point.y)/2;
                float dist = distance(center_point);
                if( DISTANCE > dist){
                    nearest_segment = i;
                    DISTANCE = dist;
                    nearest_segment_center_point_ = center_point;
                }
            }
            cout << "center point.x : " << center_point.x << endl << "center_point.y : " << center_point.y << endl;
        }
        
        void visualization_waypoint(){
            visualization_msgs::Marker marker;
            marker.header.frame_id = "laser";
            marker.header.stamp = ros::Time();
            marker.ns = "waypoint";
            marker.id = 0;
            marker.type = visualization_msgs::Marker::SPHERE;
            marker.action = visualization_msgs::Marker::ADD;
            marker.pose.position.x = WayPoint_Marker.x;
            marker.pose.position.y = WayPoint_Marker.y;
            marker.pose.position.z = 0;
            marker.pose.orientation.x = 0.0;
            marker.pose.orientation.y = 0.0;
            marker.pose.orientation.z = 0.0;
            marker.pose.orientation.w = 1.0;
            marker.scale.x = 0.3;
            marker.scale.y = 0.3;
            marker.scale.z = 0.3;
            marker.color.a = 1.0; // Don't forget to set the alpha!
            marker.color.r = 1.0;
            marker.color.g = 0.0;
            marker.color.b = 0.0;
            //only if using a MESH_RESOURCE marker type:
            vis_pub_.publish(marker);   
        }
        float calSteeringAngle(geometry_msgs::Point waypoint){
            float angle = atan2(waypoint.y, waypoint.x-0.5)*180/M_PI;   //  TODO 각도 다시 생각해보기
            // atan2로 구한 값은 radian 단위이기 때문에 degree로 변환해주기 위해 180/M_PI를 합니다. atan()함수도 있지만, atan2()함수가 여러상황을 고려할때 정확하므로 사용
            float steering = -int((20/M_PI)*angle)+2; // 25에서 20으로 변경
            if (steering <= MIN_STEERING) steering = MIN_STEERING;  
            else if (steering >= MAX_STEERING) steering = MAX_STEERING;
            return steering;
        }
        
        void obstacle_left_cb(obstacle_detector::Obstacles raw_obstacles_left){
            left_obstacle = raw_obstacles_left;
        }
        
        void obstacle_right_cb(obstacle_detector::Obstacles raw_obstacles_right){
            right_obstacle = raw_obstacles_right;
        }
        
        void obstacle_cb(obstacle_detector::Obstacles raw_obstacle){
            acker_data.drive.speed = 1;
            // TODO:
            //findNearestSegment(raw_obstacle);
            if (sizeof(left_obstacle.segments) == 0 || sizeof(right_obstacle.segments) == 0) {
                return;
            }
            ///////////////////////////               
            if (STATUS == 0){
                //acker_data.drive.speed = 3;
                //find nearest obstacle point
                ROS_INFO("STATUS = 0");
                findNearestSegment(raw_obstacle);
                float obstacle_dist_0 = distance(nearest_segment_center_point_);
                acker_data.drive.steering_angle = 0; //calSteeringAngle(nearest_segment_center_point_);
                //cout << "acker_data_steering_angle " << acker_data.drive.steering_angle << endl;
                if(status_flag_) cout<< " status_flag_ = true"<<endl;
                if (obstacle_dist_0 <= SeekDistance) STATUS = 1;    // 가장 가까운 장애물이 SeekDistance 보다 작아지면 STATUS = 1 로 변환합니다.
            }
            else if (STATUS == 1){
                //acker_data.drive.speed = 3;
                obstacle_detector::SegmentObstacle segment;
                ROS_INFO("STATUS = 1");
                int count_seg_1=0;
                  
                cur_obstacle = (status_flag_ == false) ? left_obstacle : right_obstacle;  // 삼항연산자를 이용하여 왼쪽에 있으면 left , 아니면 right로 구분.
                findNearestSegment(cur_obstacle);
                for (auto i : cur_obstacle.segments){// TODO: 다른부분
                    segment.first_point.x += i.first_point.x;
                    segment.last_point.x += i.last_point.x;
                    segment.first_point.y += i.first_point.y;
                    segment.last_point.y += i.last_point.y;
                    count_seg_1++;
                }
                if(count_seg_1 == 0) return;    // 수정  
                segment.first_point.x = segment.first_point.x/count_seg_1;
                segment.last_point.x = segment.last_point.x/count_seg_1;
                segment.first_point.y = segment.first_point.y/count_seg_1;
                segment.last_point.y = segment.last_point.y/count_seg_1;
                
                 
                float tempAngle = 0;    // 로컬 변수 선언과 동시에 초기화
                
                geometry_msgs::Point data_1;
                data_1.x = segment.last_point.x - segment.first_point.x;
                data_1.y = segment.last_point.y - segment.first_point.y;
                data_1.z = 0;
                SegmentVector = data_1;
                if (INIT == 0) tempAngle = innerProduct(SegmentVector);     // 초기에 INIT이 0으로 설정되었으므로 처음 실행될때 쓰인다.
                else tempAngle = angle;         
                angle = innerProduct(SegmentVector);
                INIT = 1;   // STATUS 1 이 실행되면 INIT=1로 변함
                acker_data.drive.steering_angle = (status_flag_ == false) ? 25 : -25;  // 장애물 중점이 왼쪽에 있다면 25 아니면 -25를 전달.
                //cout <<"acker steering angle : " << acker_data.drive.steering_angle << endl;
                cout<<"angle : " << angle << endl <<"tempAngle : " << tempAngle << endl;
                if (abs(angle - tempAngle) >= ANGLE){           // 이전 tempAngle 과 현재 angle 값 비교
                    STATUS = 2;
                    nearest_segment_center_point_.x = 100; 
                    nearest_segment_center_point_.y = 100; 
                    nearest_segment_center_point_.z = 0;
                    DISTANCE = 100;    
                    INIT = 0;   // STATUS=2가 될때 INIT=0으로 변함.
                    cout<< "Status Change" << endl;
                }
            }
            else if (STATUS == 2){
                wayPoint.x=0;
                wayPoint.y=0;
                //acker_data.drive.speed = 2.0;
                ROS_INFO("STATUS = 2");
                int count_seg_2=0;
                cur_obstacle = (status_flag_ == false) ? left_obstacle : right_obstacle;
                
                for (auto i : cur_obstacle.segments){
                    wayPoint.x += i.first_point.x;
                    wayPoint.x += i.last_point.x;
                    wayPoint.y += i.first_point.y;
                    wayPoint.y += i.last_point.y;
                    count_seg_2++;
                }
                if(count_seg_2==0) return;      // 수정
                wayPoint.x = wayPoint.x/count_seg_2; // TODO: !!!!!!!!!!!!!!!
                if (wayPoint.x >= 3) wayPoint.x = 3; //TODO 나중에 고쳐보기
                cout << "count_seg_2 : " << count_seg_2 << endl;
                wayPoint.y = (status_flag_ == false) ? wayPoint.y/count_seg_2 - GAP : wayPoint.y/count_seg_2 + GAP;
                // 장애물 중점이 왼쪽에 있다면 GAP을 빼주어 오른쪽에 WayPoint를 설정하고, 아니라면 반대로 한다.
                WayPoint_Marker = wayPoint;
                
                acker_data.drive.steering_angle = calSteeringAngle(wayPoint)/3;
                cout << "wayPoint.x : " << wayPoint.x << endl;
                cout << "wayPoint.y : " << wayPoint.y << endl;
                //cout << "a : " << acker_data.drive.steering_angle << endl;
                //ROS_INFO("WAYPOINT : " WayPoint);
                // 장애물과 수직거리가 0.8 안에 해당하면 STATUS 3으로 이동
                if (wayPoint.x < 0.8)  { // TODO 더 괜찮은 방식 조건 생각하기
                    STATUS = 3;
                    geometry_msgs::Point data_2;
                    data_2.x = 100;
                    data_2.y = 100;
                    data_2.z = 0;
                    nearest_segment_center_point_ = data_2;
                    DISTANCE = 100;
                    cout<<"STATUS change"<<endl;
                }
                // 만약 이전에 STATUS 2가 실행된적이 있다면 STATUS 4로 넘어간다. 정적 장애물이 2개인것을 기준으로 짜여졌기 때문에
            
                
            }
            else if (STATUS == 3){          //STATUS 0과 유사
                //acker_data.drive.speed = 2.5;
                ROS_INFO("STATUS = 3");
                findNearestSegment(raw_obstacle);
                float obstacle_dist_3 = distance(nearest_segment_center_point_);
                cout << "obstacle_dist_3 : " << obstacle_dist_3 << endl;
                nearest_segment_center_point_.y = nearest_segment_center_point_.y + 0.3;            // 0.3 을 더해준다음에 steer를 계산하는 이유?
                acker_data.drive.steering_angle = calSteeringAngle(nearest_segment_center_point_);
                //cout << "steering angle : " << acker_data.drive.steering_angle << endl;
                wayPoint.x = nearest_segment_center_point_.x;
                wayPoint.y = nearest_segment_center_point_.y;
                WayPoint_Marker = wayPoint;
                //ROS_INFO("STATUS : %d", STATUS);
                //ROS_INFO("DISTANCE : %f", distance);
                if (obstacle_dist_3 < 5 && status_flag_ ==false)
                {
                    STATUS = 0;
                    DISTANCE = 100.0;
                    status_flag_ = true;
                }
                
                if(obstacle_dist_3 > 150 && status_flag_ == true) STATUS = 4;
            }
            //FINISH STATUS
            
            else if (STATUS == 4 ){
                ROS_INFO("STATUS = 4");
                acker_data.drive.speed = 0;
                acker_data.drive.steering_angle = 0;
                ROS_INFO("END");
            }
            cout <<"steering angle : " << acker_data.drive.steering_angle << endl;
            cout<<"nearest_segment_center_point_ " << nearest_segment_center_point_ << endl;
            //ROS_INFO("STATUS : %d", (STATUS);
            //ROS_INFO("SPEED : %d",  (acker_data.drive.speed));
            //ROS_INFO("STEER : %d",  (acker_data.drive.steering_angle));
    }   
};
int main(int argc, char **argv){
    ros::init(argc, argv, "static_avoidance");  
    StaticAvoidance staticAvoidance;        // 이 부분에서 생성자가 실행되고 start()가 작동함.
    uint32_t shape = visualization_msgs::Marker::CUBE;
    while(ros::ok()){
        //staticAvoidance.exect();    // INIT==0 이면 직진
        staticAvoidance.acker_pub.publish(staticAvoidance.getAckerData());      // acker_data를 받아온다. speed, steering_angle
        staticAvoidance.visualization_waypoint();
        //cout <<"steering angle : " << staticAvoidance.getAckerData().drive.steering_angle << endl;
        ros::spinOnce();        
    }
    
    return 0;
}
