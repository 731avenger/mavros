/**
 * @brief RC plugin
 * @file rc.cpp
 * @author Vladimir Ermakov <vooon341@gmail.com>
 *
 * @addtogroup plugin
 */
/*
 * Copyright 2014 Vladimir Ermakov.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <mavros/mavros_plugin.h>
#include <pluginlib/class_list_macros.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <mavros/RCIn.h>
#include <mavros/RCOut.h>
#include <mavros/OverrideRCIn.h>

namespace mavplugin {

/**
 * @brief RC IO plugin
 */
class RCIOPlugin : public MavRosPlugin {
public:
	RCIOPlugin() :
		raw_rc_in(0),
		raw_rc_out(0),
		has_rc_channels_msg(false)
	{ };

	void initialize(UAS &uas_,
			ros::NodeHandle &nh,
			diagnostic_updater::Updater &diag_updater)
	{
		uas = &uas_;

		rc_nh = ros::NodeHandle(nh, "rc");
		rc_in_pub = rc_nh.advertise<mavros::RCIn>("in", 10);
		rc_out_pub = rc_nh.advertise<mavros::RCOut>("out", 10);
		override_srv = rc_nh.advertiseService("override", &RCIOPlugin::override_cb, this);

		uas->sig_connection_changed.connect(boost::bind(&RCIOPlugin::connection_cb, this, _1));
	};

	std::string get_name() {
		return "RCIO";
	};

	std::vector<uint8_t> get_supported_messages() {
		return {
			MAVLINK_MSG_ID_RC_CHANNELS_RAW,
			MAVLINK_MSG_ID_RC_CHANNELS,
			MAVLINK_MSG_ID_SERVO_OUTPUT_RAW
		};
	};

	void message_rx_cb(const mavlink_message_t *msg, uint8_t sysid, uint8_t compid) {
		switch (msg->msgid) {
		case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
			mavlink_rc_channels_raw_t iport;
			mavlink_msg_rc_channels_raw_decode(msg, &iport);
			handle_rc_channels_raw(msg, iport);
			break;

		case MAVLINK_MSG_ID_RC_CHANNELS:
			mavlink_rc_channels_t channels;
			mavlink_msg_rc_channels_decode(msg, &channels);
			handle_rc_channels(msg, channels);
			break;

		case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:
			mavlink_servo_output_raw_t oport;
			mavlink_msg_servo_output_raw_decode(msg, &oport);
			handle_servo_output_raw(msg, oport);
			break;
		}
	};

private:
	boost::recursive_mutex mutex;
	UAS *uas;

	std::vector<uint16_t> raw_rc_in;
	std::vector<uint16_t> raw_rc_out;
	bool has_rc_channels_msg;

	ros::NodeHandle rc_nh;
	ros::Publisher rc_in_pub;
	ros::Publisher rc_out_pub;
	ros::ServiceServer override_srv;

	/* -*- rx handlers -*- */

	void handle_rc_channels_raw(const mavlink_message_t *msg,
			mavlink_rc_channels_raw_t &port) {
		boost::recursive_mutex::scoped_lock lock(mutex);

		/* if we receive RC_CHANNELS, drop RC_CHANNELS_RAW */
		if (has_rc_channels_msg)
			return;

		size_t offset = port.port * 8;
		if (raw_rc_in.size() < offset + 8)
			raw_rc_in.resize(offset + 8);

#define SET_RC_IN(mavidx)	\
		raw_rc_in[offset + mavidx - 1] = port.chan ## mavidx ## _raw
		SET_RC_IN(1);
		SET_RC_IN(2);
		SET_RC_IN(3);
		SET_RC_IN(4);
		SET_RC_IN(5);
		SET_RC_IN(6);
		SET_RC_IN(7);
		SET_RC_IN(8);
#undef SET_RC_IN

		mavros::RCInPtr rcin_msg(new mavros::RCIn);

		rcin_msg->header.stamp = ros::Time::now();
		rcin_msg->rssi = port.rssi;
		rcin_msg->channels = raw_rc_in;

		rc_in_pub.publish(rcin_msg);
	}

	void handle_rc_channels(const mavlink_message_t *msg,
			mavlink_rc_channels_t &channels) {
		boost::recursive_mutex::scoped_lock lock(mutex);

		has_rc_channels_msg = true;

		if (channels.chancount > 18) {
			raw_rc_in.resize(18);
			ROS_WARN_THROTTLE_NAMED(60, "rc",
					"FCU receives %u RC channels, but RC_CHANNELS can store 18",
					channels.chancount);
		}
		else
			raw_rc_in.resize(channels.chancount);

#define IFSET_RC_IN(mavidx)				\
		if (channels.chancount >= mavidx)	\
			raw_rc_in[mavidx-1] = channels.chan ## mavidx ## _raw
		IFSET_RC_IN(1);
		IFSET_RC_IN(2);
		IFSET_RC_IN(3);
		IFSET_RC_IN(4);
		IFSET_RC_IN(5);
		IFSET_RC_IN(6);
		IFSET_RC_IN(7);
		IFSET_RC_IN(8);
		IFSET_RC_IN(9);
		IFSET_RC_IN(10);
		IFSET_RC_IN(11);
		IFSET_RC_IN(12);
		IFSET_RC_IN(13);
		IFSET_RC_IN(14);
		IFSET_RC_IN(15);
		IFSET_RC_IN(16);
		IFSET_RC_IN(17);
		IFSET_RC_IN(18);
#undef IFSET_RC_IN

		mavros::RCInPtr rcin_msg(new mavros::RCIn);

		rcin_msg->header.stamp = ros::Time::now();
		rcin_msg->rssi = channels.rssi;
		rcin_msg->channels = raw_rc_in;

		rc_in_pub.publish(rcin_msg);
	}

	void handle_servo_output_raw(const mavlink_message_t *msg,
			mavlink_servo_output_raw_t &port) {
		boost::recursive_mutex::scoped_lock lock(mutex);

		size_t offset = port.port * 8;
		if (raw_rc_out.size() < offset + 8)
			raw_rc_out.resize(offset + 8);

#define SET_RC_OUT(mavidx)	\
		raw_rc_out[offset + mavidx - 1] = port.servo ## mavidx ## _raw
		SET_RC_OUT(1);
		SET_RC_OUT(2);
		SET_RC_OUT(3);
		SET_RC_OUT(4);
		SET_RC_OUT(5);
		SET_RC_OUT(6);
		SET_RC_OUT(7);
		SET_RC_OUT(8);
#undef SET_RC_OUT

		mavros::RCOutPtr rcout_msg(new mavros::RCOut);

		rcout_msg->header.stamp = ros::Time::now();
		rcout_msg->channels = raw_rc_out;

		rc_out_pub.publish(rcout_msg);
	}

	/* -*- low-level send functions -*- */

	void rc_channels_override(boost::array<uint16_t, 8> &channels) {
		mavlink_message_t msg;

		mavlink_msg_rc_channels_override_pack(0, 0, &msg,
				uas->get_tgt_system(),
				uas->get_tgt_component(),
				channels[0],
				channels[1],
				channels[2],
				channels[3],
				channels[4],
				channels[5],
				channels[6],
				channels[7]
				);
		uas->mav_link->send_message(&msg);
	}

	/* -*- callbacks -*- */

	void connection_cb(bool connected) {
		boost::recursive_mutex::scoped_lock lock(mutex);
		raw_rc_in.clear();
		raw_rc_out.clear();
		has_rc_channels_msg = false;
	}

	bool override_cb(mavros::OverrideRCIn::Request &req,
			mavros::OverrideRCIn::Response &res) {

		rc_channels_override(req.channels);
		res.success = true;
		return true;
	}
};

}; // namespace mavplugin

PLUGINLIB_EXPORT_CLASS(mavplugin::RCIOPlugin, mavplugin::MavRosPlugin)
