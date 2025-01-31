#include <turtle_pong/bomb.h>


cBomb::cBomb(ros::NodeHandle &nh)
    : nh_(nh)
{
    ROS_INFO("Initialize Bomb!");

    reset();

    // ball_pose_sub_ = nh_.subscribe<turtlesim::Pose>("/ball/pose", 1, &cBomb::poseCallbackBall, this);
    bomb_pose_sub_ = nh_.subscribe<turtlesim::Pose>("/bomb/pose", 1, &cBomb::poseCallbackBomb, this);
    pose_left_sub_ = nh_.subscribe<turtlesim::Pose>("/turtle_left/pose", 1, &cBomb::poseLeftCallback, this);
    pose_right_sub_ = nh_.subscribe<turtlesim::Pose>("/turtle_right/pose", 1, &cBomb::poseRightCallback, this);
    bomb_cmd_vel_pub_ = nh_.advertise<geometry_msgs::Twist>("/bomb/cmd_vel", 1);
    teleport_abs_client_ = nh_.serviceClient<turtlesim::TeleportAbsolute>("/bomb/teleport_absolute");

    direction_ = STOP;
};

void cBomb::penOff(bool off)
{
    turtlesim::SetPen set_pen;
    set_pen.request.off = off;
    ros::service::call("/bomb/set_pen", set_pen);
}

void cBomb::updateDirection()
{
    double theta = pose_->theta;
    if (0.0 == theta)
    {
        direction_ = RIGHT;
    }
    else if (0.0 < theta && theta < M_PI_2)
    {
        direction_ = UP_RIGHT;
    }
    else if (M_PI_2 < theta && theta < M_PI)
    {
        direction_ = UP_LEFT;
    }
    else if (M_PI == theta)
    {
        direction_ = LEFT;
    }
    else if (-M_PI < theta && theta < -M_PI_2)
    {
        direction_ = DOWN_LEFT;
    }
    else if (-M_PI_2 < theta && theta < 0.0)
    {
        direction_ = DOWN_RIGHT;
    }
}


void cBomb::move()
{

    if (pose_ == 0 || pose_left_ == 0 || pose_right_ == 0)
    {
        ROS_INFO("Pose not yet received");
        setPoseAbs(1.0, 1.0, 0.5);
        
        //direction_ = DOWN_RIGHT;
        return;
    }    

    setVel(3, 0.0);

    // get current pose
    double x = pose_->x;
    double y = pose_->y;
    double theta = pose_->theta;


    checkPlayerCollision();
    // checkBallCollision();
    updateDirection();

    // Update pose if bomb hits the wall
    double new_theta = 0.0;
    if (11.0 < pose_->y) // hit top wall
    {
        if (direction_ == UP_LEFT)
        {
            new_theta = 2.0*M_PI - theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = DOWN_LEFT;
        }
        if (direction_ == UP_RIGHT)
        {
            new_theta = 2.0*M_PI-theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = DOWN_RIGHT;
        }
    }

    if (1e-3 > pose_->y) // hit bottom wall
    {
        if (direction_ == DOWN_LEFT)
        {
            new_theta = -theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = UP_LEFT;
        }
        if (direction_ == DOWN_RIGHT)
        {
            new_theta = -theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = UP_RIGHT;
        }
    }

    if (11.0 < pose_->x) // hit right wall
    {
        if (direction_ == UP_RIGHT)
        {
            new_theta = M_PI - theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = UP_LEFT;
        }
        if (direction_ == DOWN_RIGHT)
        {
            new_theta = -M_PI-theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = DOWN_LEFT;
        }
    }
    
    if (1e-3 > pose_->x) // hit left wall
    {
        if (direction_ == UP_LEFT)
        {
            new_theta = M_PI - theta;
            setPoseAbs(x, y, new_theta);
            //direction_ = UP_RIGHT;
        }
        if (direction_ == DOWN_LEFT)
        {
            new_theta = -(M_PI + theta);
            setPoseAbs(x, y, new_theta);
            //direction_ = DOWN_RIGHT;
        }
    }
};


void cBomb::checkPlayerCollision()
{
    double bomb_x = pose_->x;
    double bomb_y = pose_->y;
    double bomb_theta = pose_->theta;

    double left_x = pose_left_->x;
    double left_y = pose_left_->y;
    double left_theta = pose_left_->theta;

    double right_x = pose_right_->x;
    double right_y = pose_right_->y;
    double right_theta = pose_right_->theta;

    double delta_x = 0.2;
    double delta_y = 0.5;
    double paddle_size = 2.0* delta_y;
    double max_bounce_angle = 5*M_PI/12;

    if (left_x - delta_x < bomb_x && bomb_x < left_x + delta_x)
    {
        if (left_y - delta_y < bomb_y && bomb_y < left_y + delta_y)
        {
            // https://gamedev.stackexchange.com/questions/4253/in-pong-how-do-you-calculate-the-balls-direction-when-it-bounces-off-the-paddl#:~:text=Take%20the%20relative%20intersection%20and,12%20radians%20(75%20degrees).
            double rel_intersect_y = (left_y + delta_y) - bomb_y;
            double norm_rel_intersect_y = rel_intersect_y / paddle_size;
            double bounce_angle = norm_rel_intersect_y * max_bounce_angle;
            ROS_INFO("BOMB EXPLODED FOR LEFT PLAYER");
            setPoseAbs(bomb_x, bomb_y, bounce_angle);
        }
    }


    if (right_x - delta_x < bomb_x && bomb_x < right_x + delta_x)
    {
        if (right_y - delta_y < bomb_y && bomb_y < right_y + delta_y)
        {
            double rel_intersect_y = (right_y + delta_y) - bomb_y;
            double norm_rel_intersect_y = rel_intersect_y / paddle_size;
            double bounce_angle = norm_rel_intersect_y * max_bounce_angle;
            if (bounce_angle > 0)
                bounce_angle += M_PI_2;
            else
                bounce_angle -= M_PI_2;
            
            ROS_INFO("BOMB EXPLODED FOR RIGHT PLAYER");
            setPoseAbs(bomb_x, bomb_y, bounce_angle);
        }
    }
}


void cBomb::poseCallbackBomb(const turtlesim::PoseConstPtr& pose)
{
    pose_ = pose;
    ROS_INFO_THROTTLE(1, "x: %f, y: %f, theta: %f, linear_vel: %f, angular_vel: %f, dir: %i", 
            pose_->x, pose_->y, pose_->theta, pose_->linear_velocity, pose_->angular_velocity, direction_);
};

void cBomb::poseLeftCallback(const turtlesim::PoseConstPtr& pose)
{
    pose_left_ = pose;
    ROS_DEBUG_THROTTLE(1, "Left: x: %f, y: %f, theta: %f, linear_vel: %f, angular_vel: %f", 
            pose->x, pose->y, pose->theta, pose->linear_velocity, pose->angular_velocity);
};

void cBomb::poseRightCallback(const turtlesim::PoseConstPtr& pose)
{
    pose_right_ = pose;
    ROS_DEBUG_THROTTLE(1, "Left: x: %f, y: %f, theta: %f, linear_vel: %f, angular_vel: %f", 
            pose->x, pose->y, pose->theta, pose->linear_velocity, pose->angular_velocity);
};

void cBomb::reset()
{
    setPoseAbs(1.0, 1.0, 0.0);
    penOff(true);
}

void cBomb::setPoseAbs(double x, double y, double theta)
{
    turtlesim::TeleportAbsolute pose_abs;
    pose_abs.request.x = x;
    pose_abs.request.y = y;
    pose_abs.request.theta = theta;
    ROS_INFO("Set Absolute Pose for Bomb (x, y, theta) = (%f, %f, %f)", x, y, theta);
    teleport_abs_client_.call(pose_abs);
}

void cBomb::setVel(double x, double theta)
{
    geometry_msgs::Twist twist;
    twist.linear.x = x;
    twist.angular.z = theta;
    bomb_cmd_vel_pub_.publish<geometry_msgs::Twist>(twist);
}

double cBomb::randomAngle()
{
    double angle = ((double)rand() / (double)RAND_MAX);
    double r = ((double)rand() / (double)RAND_MAX);
    if (r > 0.5)
    {
        return angle * M_PI;
    }
    else
    {
        return angle * -M_PI;
    }
    
}


int main(int argc, char** argv)
 {
    ros::init(argc, argv, "turtle_pong_bomb");

    ros::NodeHandle nh;

    turtlesim::Spawn spawn;
    spawn.request.name = "bomb";
    spawn.request.x = 5.0;
    spawn.request.y = 6.0;
    spawn.request.theta = 0.0;
    
    ROS_INFO("Spawn bomb turtle");
    ros::service::call<turtlesim::Spawn>("/spawn", spawn);
    

    cBomb bomb(nh);

    ros::Rate loop_rate(100);
    
    while(ros::ok())
    {
        bomb.move();
        ros::spinOnce();
        loop_rate.sleep();
        
    }
}