#include "recorder.h"
#include "log.h"
#include "string-util.h"
#include "player.h"
#include "stream.h"

using namespace std;
using namespace cxm::util;

namespace cxm {
namespace av {

Recorder::Recorder(shared_ptr<Player> player) :
	mplayer(player), mcontext(NULL), misRun(false)
{
}

Recorder::~Recorder()
{
	if (NULL != mcontext)
		Stop();
}

int Recorder::Start(const string &fileName, int recordTime)
{
	if (NULL != mcontext) {
		LOGE("Already start");
		return -1;
	}
	LOGD("Start record player: %s %d", fileName.c_str(), recordTime);

	do {
		// format
		vector<string> suffixes;
		StringUtil::Split(fileName, '.', suffixes);
		AVOutputFormat *format = av_guess_format(suffixes.back().c_str(), NULL, NULL);
		if (NULL == format) {
			LOGE("Cannot guess format by suffix: %s", suffixes.back().c_str());
			break;
		}
		
		// context
		mcontext = avformat_alloc_context();
		if (NULL == mcontext) {
			LOGE("Cannot alloc format context");
			break;
		}
		mcontext->oformat = format;
		int res = avio_open2(&mcontext->pb, fileName.c_str(), AVIO_FLAG_WRITE, NULL, NULL);
		if (res < 0) {
			LOGE("Cannot open avio: %d", res);
			break;
		}

		// get recording stream
		int streamSize = mplayer->GetStreamSize();
		if (streamSize < 1) {
			LOGE("Cannot get stream size: %d", streamSize);
			break;
		}
		AVStream *pstream = mplayer->GetStream(0)->GetStream();
		AVCodecContext *streamContext = mplayer->GetStream(0)->GetContext();
		// new stream
		AVStream *outStream = avformat_new_stream(mcontext, NULL);
		if (NULL == outStream) {
			LOGE("Cannot open stream");
			break;
		}
		res = avcodec_copy_context(outStream->codec, streamContext);
		if (0 != res) {
			LOGE("Cannot copy context: %d", res);
			break;
		}
		outStream->sample_aspect_ratio.num = streamContext->sample_aspect_ratio.num;
		outStream->sample_aspect_ratio.den = streamContext->sample_aspect_ratio.den;
		outStream->r_frame_rate = pstream->r_frame_rate;
		outStream->avg_frame_rate = outStream->r_frame_rate;
		outStream->time_base = av_inv_q(outStream->r_frame_rate);
		outStream->codec->time_base = outStream->time_base;
		res = avformat_write_header(mcontext, NULL);
		if (0 != res) {
			LOGE("Cannot write header: %d", res);
			break;
		}
		av_dump_format(mcontext, 0, fileName.c_str(), 1);

		// register notify
		mplayer->SetPlayerProcdule(this, NULL);
		// start thread
		misRun = true;
		mthread = shared_ptr<Thread>(new Thread(this, "recorder"));
		mthread->Start();

		return 0;
	} while (0);

	Stop();
	return -1;
}

void Recorder::Stop()
{
	if (NULL == mcontext)
		return;

	// unregist notify
	mplayer->RemovePlayerProcdule(this);

	// stop thread
	if (NULL != mthread.get()) {
		misRun = false;
		msafeQueue.NotifyAll();
		mthread->Join();
		mthread.reset();
	}

	int res = av_write_trailer(mcontext);
	if (0 != res)
		LOGE("Cannot write trailer: %d", res);
	res = avio_close(mcontext->pb);
	if (0 != res)
		LOGE("Cannot close avio: %d", res);
	avformat_free_context(mcontext);
	mcontext = NULL;
}

void Recorder::Run()
{
	AVStream *pstream = mplayer->GetStream(0)->GetStream();
	int res = 0;
	int pts = 0;

	while (misRun) {
		shared_ptr<MyAVPacket> packet = msafeQueue.Get();
		if (!misRun)
			return; // double check cause safe queue blocked

		AVPacket &pkt = packet->GetPacket();
		LOGE("Packet flag: %d", pkt.flags);
		if (pts == 0 && !(pkt.flags & AV_PKT_FLAG_KEY)) {
			LOGE("Skip frame before key");
			continue;
		}

		AVPacket pkt2;
		av_init_packet(&pkt2);
		pkt2.stream_index = 0;
		// uint8_t *data = new uint8_t[pkt.size];
		// memcpy(data, pkt.data, pkt.size);
		pkt2.data = pkt.data;
		pkt2.size = pkt.size;
		// pkt2.flags = pkt.flags;
		pkt2.pts = pkt2.dts = pts;
		pts += 300;
		
		// pkt2.pts = av_rescale_q(pkt.pts, pstream->codec->time_base, mcontext->streams[0]->time_base);
		// pkt2.dts = av_rescale_q(pkt.dts, pstream->codec->time_base, mcontext->streams[0]->time_base);
		// pkt.pts = av_rescale_q(pkt.pts, pstream->codec->time_base, mcontext->streams[0]->time_base);
		// pkt.dts = av_rescale_q(pkt.dts, pstream->codec->time_base, mcontext->streams[0]->time_base);

		res = av_interleaved_write_frame(mcontext, &pkt2);
		// delete []data;
		// av_free_packet(&pkt2);
		if (0 != res)
			LOGE("Cannot write frame: %d", res);
	}
}
	
int Recorder::OnPlayerProcdule(Player &player, void *procduleTag,
		CXM_PLAYER_EVENT event, shared_ptr<object> args)
{
	switch (event) {
	case CXM_PLAYER_EVENT_GET_PACKET:
		shared_ptr<MyAVPacket> packet = dynamic_pointer_cast<MyAVPacket>(args);
		if (NULL == packet.get()) {
			LOGE("Cannot convert packet");
			break;
		}

		msafeQueue.Put(packet);
		break;
	}
	return 0;
}

}
}