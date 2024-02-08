#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <pthread.h>
#include <sched.h>


#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

rcl_publisher_t publisher;
std_msgs__msg__Int32 msg;

void * publisher_thread(void * args)
{
	rcl_publisher_t * pub = (rcl_publisher_t *) args;

	std_msgs__msg__Int32 msg2 = {0};
    	// pthread_t ptid = pthread_self();

	while (1)
	{
		for (size_t i = 0; i < 10; i++)
		{
			RCSOFTCHECK(rcl_publish(pub, &msg2, NULL));
			// printf("Sent: %d (thread: %ld)\n", msg.data, ptid);
			msg2.data++;
			//sleep(1);
		}
		usleep(100000);
		//sched_yield();
	}
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time);
	if (timer != NULL) {
		RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
		msg.data++;
	}
}

void appMain(void * arg)
{	
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node;
	RCCHECK(rclc_node_init_default(&node, "multithreaded_node", "", &support));
	
	// create publishers
	rcl_publisher_t publishers[2];

	RCCHECK(rclc_publisher_init_default(
		&publishers[0],
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
		"multithreaded_topic_1"));
	
	RCCHECK(rclc_publisher_init_best_effort(
		&publishers[1],
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
		"multithreaded_topic_2"));
		
	// create publisher
	RCCHECK(rclc_publisher_init_default(
		&publisher,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
		"freertos_int32_publisher"));
		
	// create timer,
	rcl_timer_t timer;
	const unsigned int timer_timeout = 1000;
	RCCHECK(rclc_timer_init_default(
		&timer,
		&support,
		RCL_MS_TO_NS(timer_timeout),
		timer_callback));
		
	// create executor
	rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
	RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	msg.data = 0;


	pthread_attr_t attr[2];
    struct sched_param param[2];
	for (size_t i = 0; i < 2; i++)
	{
		pthread_attr_init(&attr[i]);
    	param[i].sched_priority = 5; // Adjust priority as needed
    	pthread_attr_setschedparam(&attr[i], &param[i]);
	}
    
	
	// Create publisher threads
	pthread_t id[2];
	for (size_t i = 0; i < sizeof(id)/sizeof(pthread_t); i++)
	{
		pthread_create(&id[i], &attr[i], publisher_thread, &publishers[i]);
	}

	while(1)
	{
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1000));
		usleep(10000);
		//sched_yield();
	}
	
	// free resources
	// RCCHECK(rcl_publisher_fini(&publisher_1, &node))
	RCCHECK(rcl_node_fini(&node))
	
	vTaskDelete(NULL);
}
