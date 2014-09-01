/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, Joshua James
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SHARED_MEMORY_SUBSCRIBER_HPP
#define SHARED_MEMORY_SUBSCRIBER_HPP

#include "shared_memory_transport.hpp"

namespace shared_memory_interface
{
  template<typename T> //T must be the type of a ros message
  class Subscriber
  {
  public:
    Subscriber(bool listen_to_rostopic = true)
    {
      m_listen_to_rostopic = listen_to_rostopic;
    }

    ~Subscriber()
    {
      m_callback_thread->interrupt();
      m_callback_thread->detach();
      delete m_callback_thread;
    }

    //just sets up names and subscriber for future getCurrent / waitFor calls
    bool subscribe(std::string topic_name, std::string shared_memory_interface_name = "smi")
    {
      m_interface_name = shared_memory_interface_name;
      configureTopicPaths(m_interface_name, topic_name, m_full_ros_topic_path, m_full_topic_path);
      m_smt.configure(m_interface_name, m_full_topic_path);

      if(m_listen_to_rostopic)
      {
        ros::NodeHandle nh("~");
        m_subscriber = nh.subscribe<T>(m_full_ros_topic_path, 1, boost::bind(&Subscriber<T>::blankCallback, this, _1));
      }

      return true;
    }

    bool subscribe(std::string topic_name, boost::function<void(T&)> callback, std::string shared_memory_interface_name = "smi")
    {
      subscribe(topic_name, shared_memory_interface_name);
      m_callback_thread = new boost::thread(boost::bind(&Subscriber<T>::callbackThreadFunction, this, &m_smt, callback));
      return true;
    }

    bool waitForSerializedROS(T& msg, double timeout = -1)
    {
      std::string serialized_data;
      if(!m_smt.awaitNewDataPolled(serialized_data, timeout))
      {
        return false;
      }

      ros::serialization::IStream istream((uint8_t*) &serialized_data[0], serialized_data.size());
      ros::serialization::deserialize(istream, msg);
      return true;
    }

    bool getCurrentSerializedROS(T& msg)
    {
      return waitForSerializedROS(msg, 0);
    }

  protected:
    SharedMemoryTransport m_smt;

    std::string m_interface_name;
    std::string m_full_topic_path;
    std::string m_full_ros_topic_path;

    bool m_listen_to_rostopic;
    ros::Subscriber m_subscriber;

    boost::thread* m_callback_thread;

    void callbackThreadFunction(SharedMemoryTransport* smt, boost::function<void(T&)> callback)
    {
      T msg;
      std::string serialized_data;
//      typedef typename ros::ParameterAdapter<T>::Message MessageType;
      while(ros::ok())
      {
        smt->awaitNewDataPolled(serialized_data);
        ros::serialization::IStream istream((uint8_t*) &serialized_data[0], serialized_data.size());
        ros::serialization::deserialize(istream, msg);
        callback(msg);
        boost::this_thread::interruption_point();
      }
    }

    void blankCallback(const typename T::ConstPtr& msg)
    {
    }
  };

}
#endif //SHARED_MEMORY_SUBSCRIBER_HPP