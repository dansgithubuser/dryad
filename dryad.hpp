#include <dyad.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace dryad{

static void onPanic(const char* message);
static void onConnected(dyad_Event* e);
static void onDestroyed(dyad_Event* e);
static void onError(dyad_Event* e);
static void onData(dyad_Event* e);

#ifndef DRYAD_DRY
	struct Boss{
		Boss(){
			dyad_atPanic(onPanic);
			dyad_init();
			dyad_setUpdateTimeout(0.0);
			thread=std::thread([this](){
				while(!quit){
					mutex.lock();
					dyad_update();
					mutex.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			});
		}

		~Boss(){
			quit=true;
			thread.join();
		}

		bool quit=false;
		std::thread thread;
		std::recursive_mutex mutex;
	};

	static Boss fBoss;
#endif

class Client{
	friend void onConnected(dyad_Event*);
	friend void onClosed(dyad_Event*);
	friend void onDestroyed(dyad_Event*);
	friend void onData(dyad_Event*);
	public:
		Client(std::string ip, int port):
			_ip(ip), _port(port), _timesConnected(0), _timesDisconnected(0)
		{ connect(); }
		void write(const std::vector<uint8_t>& v){
			fBoss.mutex.lock();
			dyad_write(_stream, v.data(), v.size());
			fBoss.mutex.unlock();
		}
		void writeSizedString(const std::string& s){
			if(!s.size()) return;
			static std::vector<uint8_t> bytes;
			bytes.resize(4+s.size());
			bytes[0]=(s.size()>>0x00)&0xff;
			bytes[1]=(s.size()>>0x08)&0xff;
			bytes[2]=(s.size()>>0x10)&0xff;
			bytes[3]=(s.size()>>0x18)&0xff;
			std::copy(s.begin(), s.end(), bytes.begin()+4);
			write(bytes);
		}
		bool readSizedString(std::string& s){
			std::lock_guard<std::recursive_mutex> lock(fBoss.mutex);
			if(_queue.size()<4) return false;
			uint32_t size=
				(_queue[0]<<0x00)|
				(_queue[1]<<0x08)|
				(_queue[2]<<0x10)|
				(_queue[3]<<0x18)
			;
			if(_queue.size()<4+size) return false;
			if(size==0) s="";
			else s=std::string((char*)&_queue[4], size);
			_queue.erase(_queue.begin(), _queue.begin()+4+size);
			return true;
		}
		std::string ip() const { return _ip; }
		int port() const { return _port; }
		unsigned timesConnected() const { return _timesConnected; }
		unsigned timesDisconnected() const { return _timesDisconnected; }
	private:
		void connect(){
			fBoss.mutex.lock();
			_stream=dyad_newStream();
			dyad_addListener(_stream, DYAD_EVENT_CONNECT, onConnected, this);
			dyad_addListener(_stream, DYAD_EVENT_DESTROY, onDestroyed, this);
			dyad_addListener(_stream, DYAD_EVENT_ERROR, onError, this);
			dyad_addListener(_stream, DYAD_EVENT_DATA, onData, this);
			dyad_connect(_stream, _ip.c_str(), _port);
			fBoss.mutex.unlock();
		}
		void queue(const uint8_t* data, unsigned size){ _queue.insert(_queue.end(), data, data+size); }
		unsigned backoff(){
			_backoff*=2;
			if(_backoff>500) _backoff=500;
			return _backoff;
		}
		void backoffReset(){ _backoff=5; }
		std::string _ip;
		int _port;
		dyad_Stream* _stream;
		std::vector<uint8_t> _queue;
		unsigned _timesConnected, _timesDisconnected;
		unsigned _backoff=5;
};

static void onPanic(const char* message){
	std::cerr<<"dyad panic: "<<message<<"\n";
	assert(false);
}

static void onConnected(dyad_Event* e){
	auto client=(Client*)e->udata;
	++client->_timesConnected;
}

static void onDestroyed(dyad_Event* e){
	auto client=(Client*)e->udata;
	std::this_thread::sleep_for(std::chrono::milliseconds(client->backoff()));
	if(client->_timesConnected>client->_timesDisconnected) ++client->_timesDisconnected;
	client->connect();
}

static void onError(dyad_Event* e){
	if(strcmp(e->msg, "could not connect to server")==0) return;
	std::cerr<<"dyad error: "<<e->msg<<"\n";
}

static void onData(dyad_Event* e){
	auto client=(Client*)e->udata;
	client->backoffReset();
	client->queue((uint8_t*)e->data, e->size);
}

}//namespace dryad
