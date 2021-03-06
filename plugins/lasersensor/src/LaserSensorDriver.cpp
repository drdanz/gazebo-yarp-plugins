/*
 * Copyright (C) 2013-2015 Fondazione Istituto Italiano di Tecnologia RBCS & iCub Facility & ADVR
 * Authors: see AUTHORS file.
 * CopyPolicy: Released under the terms of the LGPLv2.1 or any later version, see LGPL.TXT or LGPL3.TXT
 */


#include "LaserSensorDriver.h"
#include <GazeboYarpPlugins/Handler.hh>

#include <yarp/os/LogStream.h>
#include <yarp/os/LockGuard.h>
#include <gazebo/math/Vector3.hh>
#include <gazebo/sensors/sensors.hh>

using namespace yarp::dev;

const std::string YarpLaserSensorScopedName = "sensorScopedName";

GazeboYarpLaserSensorDriver::GazeboYarpLaserSensorDriver() {}
GazeboYarpLaserSensorDriver::~GazeboYarpLaserSensorDriver() {}

/**
 *
 * Export a force/torque sensor.
 *
 * \todo check forcetorque data
 */
void GazeboYarpLaserSensorDriver::onUpdate(const gazebo::common::UpdateInfo& /*_info*/)
{   
    yarp::os::LockGuard guard(m_mutex);

    /** \todo ensure that the timestamp is the right one */
    /** \todo TODO use GetLastMeasureTime, not GetLastUpdateTime */
#if GAZEBO_MAJOR_VERSION >= 7
    m_lastTimestamp.update(this->m_parentSensor->LastUpdateTime().Double());
    this->m_parentSensor->Ranges(m_sensorData);
#else
    m_lastTimestamp.update(this->m_parentSensor->GetLastUpdateTime().Double());
    this->m_parentSensor->GetRanges(m_sensorData);
#endif

    return;
}

//DEVICE DRIVER
bool GazeboYarpLaserSensorDriver::open(yarp::os::Searchable& config)
{
    yarp::os::LockGuard guard(m_mutex);

    //Get gazebo pointers
    std::string sensorScopedName(config.find(YarpLaserSensorScopedName.c_str()).asString().c_str());

    m_parentSensor = dynamic_cast<gazebo::sensors::RaySensor*>(GazeboYarpPlugins::Handler::getHandler()->getSensor(sensorScopedName));

    if (!m_parentSensor)
    {
        yError() << "Error, sensor" <<  sensorScopedName << "was not found" ;
        return  false ;
    }

    //Connect the driver to the gazebo simulation
    this->m_updateConnection = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboYarpLaserSensorDriver::onUpdate, this, _1));

    
    m_max_angle = m_parentSensor->AngleMax().Degree();
    m_min_angle = m_parentSensor->AngleMin().Degree();
    m_max_range = m_parentSensor->RangeMax();
    m_min_range = m_parentSensor->RangeMin();  
    m_samples   = m_parentSensor->RangeCount();
    m_rate      = m_parentSensor->UpdateRate();
    m_sensorData.resize(m_samples, 0.0);
    m_enable_clip_range = false;
        
    return true;
}

bool GazeboYarpLaserSensorDriver::close()
{
    if (this->m_updateConnection.get())
    {
        gazebo::event::Events::DisconnectWorldUpdateBegin(this->m_updateConnection);
        this->m_updateConnection = gazebo::event::ConnectionPtr();
    }
    return true;
}

//PRECISELY TIMED
yarp::os::Stamp GazeboYarpLaserSensorDriver::getLastInputStamp()
{
    return m_lastTimestamp;
}

bool GazeboYarpLaserSensorDriver::getMeasurementData (yarp::sig::Vector &data)
{
    yarp::os::LockGuard guard(m_mutex);
    ///< \todo TODO in my opinion the reader should care of passing a vector of the proper dimension to the driver, but apparently this is not the case
    /*
    if( (int)m_forceTorqueData.size() != YarpForceTorqueChannelsNumber ||
        (int)out.size() != YarpForceTorqueChannelsNumber ) {
        return AS_ERROR;
    }
    */

   if (m_sensorData.size() != m_samples)
   {
       m_device_status = DEVICE_GENERAL_ERROR;
       yError() << "Internal error";
       return false ;
   }

   if (data.size() != m_samples)
   {
       data.resize(m_samples);
   }

    for (unsigned int i=0; i<m_samples; i++)
    {
      if (m_enable_clip_range)
      {
        if (m_sensorData[i]>m_max_range) m_sensorData[i]=m_max_range;
        if (m_sensorData[i]<m_min_range) m_sensorData[i]=m_min_range;    
      }
      data[i] = m_sensorData[i];
    }

    m_device_status = DEVICE_OK_IN_USE;
    return true;
}

bool GazeboYarpLaserSensorDriver::getDeviceStatus (Device_status &status)
{
    yarp::os::LockGuard guard(m_mutex);
    status = m_device_status;
    return true;
}

bool GazeboYarpLaserSensorDriver::getDistanceRange (double &min, double &max)
{
    yarp::os::LockGuard guard(m_mutex);
    min = m_min_range;
    max = m_max_range;
    return true;
}

bool GazeboYarpLaserSensorDriver::setDistanceRange (double min, double max)
{
    yarp::os::LockGuard guard(m_mutex);
    yError() << "setDistanceRange() Not yet implemented";
    return false;
}

bool GazeboYarpLaserSensorDriver::getScanLimits (double &min, double &max)
{
    yarp::os::LockGuard guard(m_mutex);
    min = m_min_angle;
    max = m_max_angle;
    return true;
}

bool GazeboYarpLaserSensorDriver::setScanLimits (double min, double max)
{
    yarp::os::LockGuard guard(m_mutex);
    yError() << "setScanLimits() Not yet implemented";
    return false;
}

bool GazeboYarpLaserSensorDriver::getHorizontalResolution (double &step)
{
    yarp::os::LockGuard guard(m_mutex);
    step = fabs(m_max_angle-m_min_angle)/m_samples;
    return true;
}

bool GazeboYarpLaserSensorDriver::setHorizontalResolution (double step)
{
    yarp::os::LockGuard guard(m_mutex);
    yError() << "setHorizontalResolution() Not yet implemented";
    return false;
}

bool GazeboYarpLaserSensorDriver::getScanRate (double &rate)
{
    yarp::os::LockGuard guard(m_mutex);
    rate = m_rate; 
    return true;
}

bool GazeboYarpLaserSensorDriver::setScanRate (double rate)
{
    yarp::os::LockGuard guard(m_mutex);
    if (rate<0)
    {
      yError() << "Invalid setScanRate";
      return false;
    }
    m_parentSensor->SetUpdateRate(rate);
    m_rate = rate;
    return true;
}

bool GazeboYarpLaserSensorDriver::getDeviceInfo (yarp::os::ConstString &device_info)
{
    yarp::os::LockGuard guard(m_mutex);
    return true;
}
