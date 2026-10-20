#pragma once
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <audioclientactivationparams.h>
#include <wrl.h>
#include <fstream>
#include <vector>
#include <chrono>
namespace stalthos::engine {
// Probe-only default-endpoint loopback. Refuses/discards capture if another
// active audio session exists; no endpoint/session volume or routing changes.
class CaptureActivation final:public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,Microsoft::WRL::FtmBase,IActivateAudioInterfaceCompletionHandler>{
public:
 HANDLE completed=CreateEventW(nullptr,FALSE,FALSE,nullptr);HRESULT result=E_PENDING;Microsoft::WRL::ComPtr<IAudioClient> client;
 ~CaptureActivation(){if(completed)CloseHandle(completed);}
 HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation)override{Microsoft::WRL::ComPtr<IUnknown> object;HRESULT activation=E_FAIL;result=operation->GetActivateResult(&activation,&object);if(SUCCEEDED(result))result=activation;if(SUCCEEDED(result))result=object.As(&client);SetEvent(completed);return S_OK;}
};
class NativeAudioCapture {
 using Clock=std::chrono::steady_clock;
 Microsoft::WRL::ComPtr<IMMDevice> endpoint_;Microsoft::WRL::ComPtr<IAudioClient> client_;Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_;Microsoft::WRL::ComPtr<IAudioSessionManager2> sessions_;
 WAVEFORMATEX* format_=nullptr;std::vector<uint8_t> data_;bool uninitialize_=false,started_=false,isolated_=false;Clock::time_point start_{};
 bool alone(){Microsoft::WRL::ComPtr<IAudioSessionEnumerator> list;if(FAILED(sessions_->GetSessionEnumerator(&list)))return false;int count=0;if(FAILED(list->GetCount(&count)))return false;for(int i=0;i<count;++i){Microsoft::WRL::ComPtr<IAudioSessionControl> session;if(FAILED(list->GetSession(i,&session)))return false;AudioSessionState active;if(FAILED(session->GetState(&active)))return false;if(active!=AudioSessionStateActive)continue;Microsoft::WRL::ComPtr<IAudioSessionControl2> owner;DWORD pid=0;if(FAILED(session.As(&owner))||FAILED(owner->GetProcessId(&pid))||pid!=GetCurrentProcessId())return false;}return true;}
public:
 std::string state="disabled",endpointId;HRESULT result=S_OK,processActivationResult=E_PENDING;
 ~NativeAudioCapture(){if(started_)client_->Stop();capture_.Reset();client_.Reset();sessions_.Reset();endpoint_.Reset();if(format_)CoTaskMemFree(format_);if(uninitialize_)CoUninitialize();}
 double seconds()const{return started_?std::chrono::duration<double>(Clock::now()-start_).count():0;}
 void start(){
  result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);uninitialize_=SUCCEEDED(result);if(FAILED(result)&&result!=RPC_E_CHANGED_MODE){state="COM_unavailable";return;}
  // Prefer process-isolated capture. Probe the actual host: OS build labels
  // alone do not establish whether this capability has been backported.
  AUDIOCLIENT_ACTIVATION_PARAMS process{};process.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;process.ProcessLoopbackParams.TargetProcessId=GetCurrentProcessId();process.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  PROPVARIANT parameters{};parameters.vt=VT_BLOB;parameters.blob.cbSize=sizeof(process);parameters.blob.pBlobData=(BYTE*)&process;
  auto completion=Microsoft::WRL::Make<CaptureActivation>();Microsoft::WRL::ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
  processActivationResult=ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&parameters,completion.Get(),&operation);
  if(SUCCEEDED(processActivationResult)){if(WaitForSingleObject(completion->completed,2000)==WAIT_OBJECT_0)processActivationResult=completion->result;else processActivationResult=HRESULT_FROM_WIN32(WAIT_TIMEOUT);}
  if(SUCCEEDED(processActivationResult)){
   client_=completion->client;format_=(WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));*format_={};format_->wFormatTag=WAVE_FORMAT_PCM;format_->nChannels=2;format_->nSamplesPerSec=44100;format_->wBitsPerSample=16;format_->nBlockAlign=4;format_->nAvgBytesPerSec=176400;
   result=client_->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,5000000,0,format_,nullptr);
   if(SUCCEEDED(result))result=client_->GetService(IID_PPV_ARGS(&capture_));if(SUCCEEDED(result))result=client_->Start();
   if(SUCCEEDED(result)){isolated_=true;started_=true;start_=Clock::now();state="capturing";endpointId="process_loopback_include_own_tree";return;}
   processActivationResult=result;capture_.Reset();client_.Reset();CoTaskMemFree(format_);format_=nullptr;
  }
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> devices;result=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices));if(FAILED(result)){state="enumerator_unavailable";return;}
  result=devices->GetDefaultAudioEndpoint(eRender,eConsole,&endpoint_);if(FAILED(result)){state="output_endpoint_unavailable";return;}
  LPWSTR id=nullptr;if(SUCCEEDED(endpoint_->GetId(&id))){for(auto* p=id;*p;++p)endpointId.push_back(char(*p));CoTaskMemFree(id);}
  result=endpoint_->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,(void**)sessions_.GetAddressOf());if(FAILED(result)||!alone()){state="blocked_other_active_audio";return;}
  result=endpoint_->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client_.GetAddressOf());if(FAILED(result)){state="capture_client_unavailable";return;}
  result=client_->GetMixFormat(&format_);if(FAILED(result)){state="mix_format_unavailable";return;}
  result=client_->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,5000000,0,format_,nullptr);if(FAILED(result)){state="loopback_unavailable";return;}
  result=client_->GetService(IID_PPV_ARGS(&capture_));if(FAILED(result)){state="capture_service_unavailable";return;}
  result=client_->Start();if(FAILED(result)){state="capture_start_failed";return;}started_=true;start_=Clock::now();state="capturing";
 }
 void poll(){if(state!="capturing")return;if(!isolated_&&!alone()){state="blocked_other_active_audio";data_.clear();client_->Stop();started_=false;return;}UINT32 frames=0;result=capture_->GetNextPacketSize(&frames);if(FAILED(result)){state="packet_failed";data_.clear();return;}while(frames){BYTE* bytes=nullptr;DWORD flags=0;result=capture_->GetBuffer(&bytes,&frames,&flags,nullptr,nullptr);if(FAILED(result)){state="buffer_failed";data_.clear();return;}size_t count=size_t(frames)*format_->nBlockAlign;if(data_.size()+count>32*1024*1024){capture_->ReleaseBuffer(frames);state="capture_bound_exceeded";data_.clear();return;}if(flags&AUDCLNT_BUFFERFLAGS_SILENT)data_.insert(data_.end(),count,0);else data_.insert(data_.end(),bytes,bytes+count);capture_->ReleaseBuffer(frames);result=capture_->GetNextPacketSize(&frames);if(FAILED(result)){state="packet_failed";data_.clear();return;}}}
 bool finish(const std::filesystem::path& path){poll();if(state!="capturing"||data_.empty())return false;client_->Stop();started_=false;std::ofstream out(path,std::ios::binary);const uint32_t fmt=18+format_->cbSize,n=uint32_t(data_.size()),riff=4+8+fmt+8+n;auto u32=[&](uint32_t v){out.write((const char*)&v,4);};out.write("RIFF",4);u32(riff);out.write("WAVEfmt ",8);u32(fmt);out.write((const char*)format_,fmt);out.write("data",4);u32(n);out.write((const char*)data_.data(),n);state=out?(isolated_?"captured_own_process_tree":"captured_guarded_endpoint_mix"):"write_failed";return bool(out);}
};
}
