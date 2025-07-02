#include "htsim_pcm_packet.hpp"

PacketDB<PcmPacket> PcmPacket::_packetdb;
PacketDB<PcmAck> PcmAck::_packetdb;
PacketDB<PcmNack> PcmNack::_packetdb;
