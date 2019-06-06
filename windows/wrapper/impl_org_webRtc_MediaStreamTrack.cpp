
#ifdef __has_include
#if __has_include(<winrt/Windows.UI.Xaml.Controls.h>)
#include <winrt/Windows.UI.Xaml.Controls.h>
#endif //__has_include(<winrt/Windows.UI.Xaml.Controls.h>)
#endif //__has_include

#include "impl_org_webRtc_MediaStreamTrack.h"

#include "impl_org_webRtc_helpers.h"
#include "impl_org_webRtc_MediaElement.h"
#include "impl_org_webRtc_MediaSource.h"
#include "impl_org_webRtc_MediaConstraints.h"
#include "impl_org_webRtc_AudioTrackSource.h"
#include "impl_org_webRtc_VideoTrackSource.h"
#include "impl_org_webRtc_WebRtcLib.h"
#include "impl_org_webRtc_WebRtcFactory.h"
#include "impl_org_webRtc_VideoFrameBuffer.h"
#include "impl_org_webRtc_VideoFrameBufferEvent.h"
#include "impl_org_webRtc_enums.h"

#include "impl_org_webRtc_pre_include.h"
#include "pc/audiotrack.h"
#include "pc/videotrack.h"
#include "api/mediastreamtrackproxy.h"
#include "api/peerconnectioninterface.h"
#include "third_party/winuwp_h264/native_handle_buffer.h"
#include "impl_org_webRtc_post_include.h"

using ::zsLib::String;
using ::zsLib::Optional;
using ::zsLib::Any;
using ::zsLib::AnyPtr;
using ::zsLib::AnyHolder;
using ::zsLib::Promise;
using ::zsLib::PromisePtr;
using ::zsLib::PromiseWithHolder;
using ::zsLib::PromiseWithHolderPtr;
using ::zsLib::eventing::SecureByteBlock;
using ::zsLib::eventing::SecureByteBlockPtr;
using ::std::shared_ptr;
using ::std::weak_ptr;
using ::std::make_shared;
using ::std::list;
using ::std::set;
using ::std::map;

// borrow definitions from class
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::MediaStreamTrack::WrapperImplType, WrapperImplType);
ZS_DECLARE_TYPEDEF_PTR(WrapperImplType::WrapperType, WrapperType);
ZS_DECLARE_TYPEDEF_PTR(WrapperImplType::NativeType, NativeType);

ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::VideoTrackSource::WrapperImplType, UseVideoTrackSource);

typedef WrapperImplType::NativeTypeScopedPtr NativeTypeScopedPtr;

typedef wrapper::impl::org::webRtc::WrapperMapper<NativeType, WrapperImplType> UseWrapperMapper;

ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::WebRtcLib, UseWebrtcLib);
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::AudioTrackSource, UseAudioTrackSource);
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::VideoTrackSource, UseVideoTrackSource);
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::VideoFrameBuffer, UseVideoFrameBuffer);
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::VideoFrameBufferEvent, UseVideoFrameBufferEvent);
ZS_DECLARE_TYPEDEF_PTR(wrapper::impl::org::webRtc::IEnum, UseEnum);

//------------------------------------------------------------------------------
static UseWrapperMapper &mapperSingleton()
{
  static UseWrapperMapper singleton;
  return singleton;
}

//------------------------------------------------------------------------------
static ::webrtc::AudioTrackInterface *unproxyAudioTrack(NativeType *native)
{
  if (!native) return nullptr;
  auto converted = dynamic_cast<::webrtc::AudioTrackInterface *>(native);
  if (!converted) return nullptr;

  return WRAPPER_DEPROXIFY_CLASS(::webrtc::AudioTrack, ::webrtc::AudioTrack, converted);
}

//------------------------------------------------------------------------------
static ::webrtc::VideoTrackInterface *unproxyVideoTrack(NativeType *native)
{
  if (!native) return nullptr;
  auto converted = dynamic_cast<::webrtc::VideoTrackInterface *>(native);
  if (!converted) return nullptr;

  return WRAPPER_DEPROXIFY_CLASS(::webrtc::VideoTrack, ::webrtc::VideoTrack, converted);
}

#ifdef CPPWINRT_VERSION
//------------------------------------------------------------------------------
static void notifyAboutNewMediaSource(WrapperImplType &wrapper, winrt::Windows::Media::Core::IMediaSource const & newSource)
{
  typedef WrapperImplType::UseMediaSourceImpl UseMediaSourceImpl;
  auto source = UseMediaSourceImpl::toWrapper(newSource);
  wrapper.notifySourceChanged(source);
}
#endif //CPPWINRT_VERSION

//------------------------------------------------------------------------------
wrapper::impl::org::webRtc::MediaStreamTrack::MediaStreamTrack() noexcept :
  videoFrameProcessingQueue_(UseWebrtcLib::videoFrameProcessingQueue())
{
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaStreamTrackPtr wrapper::org::webRtc::MediaStreamTrack::wrapper_create() noexcept
{
  auto pThis = make_shared<wrapper::impl::org::webRtc::MediaStreamTrack>();
  pThis->thisWeak_ = pThis;
  return pThis;
}

//------------------------------------------------------------------------------
wrapper::impl::org::webRtc::MediaStreamTrack::~MediaStreamTrack() noexcept
{
  thisWeak_.reset();
  teardownObserver();
  mapperSingleton().remove(native_.get());
}

//------------------------------------------------------------------------------
void wrapper::impl::org::webRtc::MediaStreamTrack::wrapper_dispose() noexcept
{
  if (!native_) return;

  teardownObserver();
  mapperSingleton().remove(native_.get());
  native_ = NativeTypeScopedPtr();
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaStreamTrackPtr wrapper::org::webRtc::MediaStreamTrack::createAudioTrack(
  String label,
  wrapper::org::webRtc::AudioTrackSourcePtr source
  ) noexcept
{
  ZS_ASSERT(source);
  if (!source) return WrapperTypePtr();

  auto sourceImpl = UseAudioTrackSource::toWrapper(source);
  ZS_ASSERT(sourceImpl);
  if (!sourceImpl) return WrapperTypePtr();

  auto factoryImpl = sourceImpl->factory();
  ZS_ASSERT(factoryImpl);
  if (!factoryImpl) return WrapperTypePtr();

  auto factory = factoryImpl->peerConnectionFactory();
  ZS_ASSERT(factory);
  if (!factory) return WrapperTypePtr();

  auto nativeSource = UseAudioTrackSource::toNative(source);

  auto native = factory->CreateAudioTrack(label, nativeSource);

  return WrapperImplType::toWrapper(native);
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaStreamTrackPtr wrapper::org::webRtc::MediaStreamTrack::createVideoTrack(
  String label,
  wrapper::org::webRtc::VideoTrackSourcePtr source
  ) noexcept
{
  ZS_ASSERT(source);
  if (!source) return WrapperTypePtr();

  auto sourceImpl = UseVideoTrackSource::toWrapper(source);
  ZS_ASSERT(sourceImpl);
  if (!sourceImpl) return WrapperTypePtr();

  auto factoryImpl = sourceImpl->factory();
  ZS_ASSERT(factoryImpl);
  if (!factoryImpl) return WrapperTypePtr();

  auto factory = factoryImpl->peerConnectionFactory();
  ZS_ASSERT(factory);
  if (!factory) return WrapperTypePtr();

  auto nativeSource = UseVideoTrackSource::toNative(source);

  auto native = factory->CreateVideoTrack(label, nativeSource);

  return WrapperImplType::toWrapper(native);
}

//------------------------------------------------------------------------------
String wrapper::impl::org::webRtc::MediaStreamTrack::get_kind() noexcept
{
  ZS_ASSERT(native_);
  if (!native_) return String();
  return native_->kind();
}

//------------------------------------------------------------------------------
String wrapper::impl::org::webRtc::MediaStreamTrack::get_id() noexcept
{
  ZS_ASSERT(native_);
  if (!native_) return String();
  return native_->id();
}

//------------------------------------------------------------------------------
bool wrapper::impl::org::webRtc::MediaStreamTrack::get_enabled() noexcept
{
  ZS_ASSERT(native_);
  if (!native_) return false;
  return native_->enabled();
}

//------------------------------------------------------------------------------
void wrapper::impl::org::webRtc::MediaStreamTrack::set_enabled(bool value) noexcept
{
  ZS_ASSERT(native_);
  if (!native_) return;

  native_->set_enabled(value);
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaStreamTrackState wrapper::impl::org::webRtc::MediaStreamTrack::get_state() noexcept
{
  ZS_ASSERT(native_);
  if (!native_) return wrapper::org::webRtc::MediaStreamTrackState::MediaStreamTrackState_ended;

  return UseEnum::toWrapper(native_->state());
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaSourcePtr wrapper::impl::org::webRtc::MediaStreamTrack::get_source() noexcept
{
  zsLib::AutoLock lock(lock_);
  return source_;
}

//------------------------------------------------------------------------------
wrapper::org::webRtc::MediaElementPtr wrapper::impl::org::webRtc::MediaStreamTrack::get_element() noexcept
{
  zsLib::AutoLock lock(lock_);
  return element_;
}

//------------------------------------------------------------------------------
void wrapper::impl::org::webRtc::MediaStreamTrack::set_element(wrapper::org::webRtc::MediaElementPtr value) noexcept
{
  {
    zsLib::AutoLock lock(lock_);
    element_ = value;
  }

  autoAttachSourceToElement();
}

void wrapper::impl::org::webRtc::MediaStreamTrack::resetSource() noexcept {
  zsLib::AutoLock lock(lock_);
  refreshSource_ = true;
}

//------------------------------------------------------------------------------
void wrapper::impl::org::webRtc::MediaStreamTrack::wrapper_onObserverCountChanged(size_t count) noexcept
{
  hasObservers_ = (count > 0);
}

//------------------------------------------------------------------------------
void wrapper::impl::org::webRtc::MediaStreamTrack::wrapper_onObserveronVideoFrameCountChanged(size_t count) noexcept
{
  hasVideoFrameObservers_ = (count > 0);
}

//------------------------------------------------------------------------------
void WrapperImplType::notifySourceChanged(UseMediaSourcePtr source)
{
  {
    zsLib::AutoLock lock(lock_);
    source_ = source;
  }

  // notify subscribers of event
  onMediaSourceChanged();

  // auto attach media source to media element
  autoAttachSourceToElement();
}

//------------------------------------------------------------------------------
void WrapperImplType::autoAttachSourceToElement()
{
#pragma ZS_BUILD_NOTE("VERIFY","(mosa) verify this is correct behaviour (REMOVE IF GOOD)")

  if (!native_) return;

  UseMediaElementPtr element;
  UseMediaSourcePtr source;

  // get mapping within lock
  {
    zsLib::AutoLock lock(lock_);
    element = element_;
    source = source_;
  }

  if (!element) return;

  {
    auto queue = UseWebrtcLib::delegateQueue();
    ZS_ASSERT(queue);

    queue->postClosure([element, source]() {
      ZS_MAYBE_USED() bool didAttachment = false;
      ZS_MAYBE_USED(didAttachment);

#ifdef CPPWINRT_VERSION
      if (!didAttachment) {
        auto nativeElement = UseMediaElementImpl::toNative_winrt(element);
        if (nativeElement) {
          auto nativeSource = UseMediaSourceImpl::toNative_winrt(source);
          if (nativeSource) {
            nativeElement.SetMediaStreamSource(nativeSource);
          } else {
            nativeElement.Source(winrt::Windows::Foundation::Uri{ nullptr });
          }
          didAttachment = true;
        }
      }
#endif //CPPWINRT_VERSION

    });
  }
}

//------------------------------------------------------------------------------
void WrapperImplType::setupObserver() noexcept
{
  if (!native_) return;

  auto converted = dynamic_cast<::webrtc::VideoTrackInterface *>(native_.get());
  if (converted) {
    if (!converted) return;

    videoObserver_ = std::make_shared<WebrtcVideoObserver>(thisWeak_.lock(), UseWebrtcLib::delegateQueue());

    rtc::VideoSinkWants wants;

#pragma ZS_BUILD_NOTE("TODO","(mosa) you may want to tweak these properties -- not sure")

    // wants.rotation_applied = ;
    // wants.black_frames = ;
    // wants.max_pixel_count = ;
    // wants.target_pixel_count = ;
    // wants.max_framerate_fps = ;

    converted->AddOrUpdateSink(videoObserver_.get(), wants);
  }
}

//------------------------------------------------------------------------------
void WrapperImplType::teardownObserver() noexcept
{
  if (!native_) return;

  if (videoObserver_) {
    auto converted = dynamic_cast<::webrtc::VideoTrackInterface *>(native_.get());
    ZS_ASSERT(converted);
    if (!converted) return;

    converted->RemoveSink(videoObserver_.get());
    videoObserver_.reset();
  }
}

//------------------------------------------------------------------------------
void WrapperImplType::notifyWebrtcObserverFrame(const ::webrtc::VideoFrame& frame) noexcept
{
  auto frameBuffer = frame.video_frame_buffer();

  ZS_ASSERT(frameBuffer);
  if (!frameBuffer)
    return;

  UseVideoFrameType frameType{};

  switch (frameBuffer->type())
  {
    case ::webrtc::VideoFrameBuffer::Type::kNative:
    {
      ::webrtc::NativeHandleBuffer *nativeBuffer = reinterpret_cast<::webrtc::NativeHandleBuffer *>(frameBuffer.get());
      switch (nativeBuffer->fourCC())
      {
        case cricket::FourCC::FOURCC_H264:
        {
          frameType = UseMediaStreamSource::VideoFrameType::VideoFrameType_H264;
          break;
        }
        default:
        {
          frameType = UseMediaStreamSource::VideoFrameType::VideoFrameType_I420;
          break;
        }
      }
      break;
    }
    default:
    {
      frameType = UseMediaStreamSource::VideoFrameType::VideoFrameType_I420;
      break;
    }
  }

  if (frameType != currentFrameType_ || !firstFrameReceived_ || refreshSource_) {
    {
      zsLib::AutoLock lock(lock_);
      refreshSource_ = false;
      firstFrameReceived_ = true;
      currentFrameType_ = frameType;
      mediaStreamSource_ = UseMediaStreamSource::create(UseMediaStreamSource::CreationProperties{ frameType });
      subscription_ = mediaStreamSource_->subscribe(videoObserver_);
    }

    auto source = mediaStreamSource_->source();
    notifyAboutNewMediaSource(*this, source);
  }

  mediaStreamSource_->notifyFrame(frame);

  if (!hasVideoFrameObservers_)
    return;

  auto wrapperBuffer = UseVideoFrameBuffer::toWrapper(frameBuffer);
  auto wrapperEvent = UseVideoFrameBufferEvent::toWrapper(wrapperBuffer);

  auto pThis = thisWeak_.lock();
  if (videoFrameProcessingQueue_) {
    videoFrameProcessingQueue_->postClosure(
        [pThis, wrapperEvent]() { pThis->onVideoFrame(wrapperEvent); });
  }
}

//------------------------------------------------------------------------------
void WrapperImplType::notifyWebrtcObserverDiscardedFrame() noexcept
{
}

//------------------------------------------------------------------------------
void WrapperImplType::onWebrtcObserverResolutionChanged(
                                                        uint32_t width,
                                                        uint32_t height
                                                        ) noexcept
{
  onResolutionChanged(width, height);
}

//------------------------------------------------------------------------------
void WrapperImplType::onWebrtcObserverRotationChanged(int rotation) noexcept
{
  onRotationChanged(rotation);
}

//------------------------------------------------------------------------------
void WrapperImplType::onWebrtcObserverFrameRateChanged(float frameRate) noexcept
{
  onFrameRateChanged(frameRate);
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(NativeType *native) noexcept
{
  if (!native) return WrapperImplTypePtr();

  ::webrtc::MediaStreamTrackInterface *original {};
  if (!original) {
    original = unproxyAudioTrack(native);
  }
  if (!original) {
    original = unproxyVideoTrack(native);
  }
  if (!original) original = native;

  // search for original non-proxied pointer in map
  auto wrapper = mapperSingleton().getExistingOrCreateNew(original, [native]() {
    auto result = make_shared<WrapperImplType>();
    result->thisWeak_ = result;
    result->native_ = rtc::scoped_refptr<NativeType>(native); // only use proxy and never original pointer
    result->setupObserver();
    return result;
  });
  return wrapper;
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(NativeTypeScopedPtr native) noexcept
{
  return toWrapper(native.get());
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapperBase(NativeType *native) noexcept
{
  return toWrapper(native);
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapperBase(NativeTypeScopedPtr native) noexcept
{
  return toWrapper(native);
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(AudioNativeType *native) noexcept
{
  return toWrapperBase(native);
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(AudioNativeTypeScopedPtr native) noexcept
{
  return toWrapperBase(native.get());
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(VideoNativeType *native) noexcept
{
  return toWrapperBase(native);
}

//------------------------------------------------------------------------------
WrapperImplTypePtr WrapperImplType::toWrapper(VideoNativeTypeScopedPtr native) noexcept
{
  return toWrapperBase(native.get());
}

//------------------------------------------------------------------------------
NativeTypeScopedPtr WrapperImplType::toNative(WrapperTypePtr wrapper) noexcept
{
  if (!wrapper) return rtc::scoped_refptr<NativeType>();
  auto converted = ZS_DYNAMIC_PTR_CAST(WrapperImplType, wrapper);
  if (!converted) return rtc::scoped_refptr<NativeType>();
  return converted->native_;
}
