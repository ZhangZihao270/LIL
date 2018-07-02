#ifndef PACKETFACTORY_H
#define PACKETFACTORY_H

/*
#include "packet.h"

typedef Packet<StartRequest, StartReply> StartPacket;
typedef Packet<GetRequest, GetReply> GetPacket;
typedef Packet<PutRequest, PutReply> PutPacket;
typedef Packet<CommitRequest, CommitReply> CommitPacket;

class PacketFactory {
public:  
  static inline void free(Packet *pkt);

  template <typename T>
    static inline T* alloc(Closure* done, const T::Request* request, T::Reply* reply);
};

template <typename T>
T* PacketFactory::alloc(Closure* done, const T::Request* request, T::Reply* reply) {
  void *ptr = ::malloc(sizeof(T));
  T *ret = new(ptr) T (done, request, reply);
  return ret; 
}

void PacketFactory::free(Packet *pkt) {
  pkt->~Packet();
  ::free(pkt);
}
*/

#endif
