###############################################################################
# Velvet
LOCAL_PATH := $(call my-dir)

my_archs := arm arm64 x86
my_src_arch := $(call get-prebuilt-src-arch, $(my_archs))

include $(CLEAR_VARS)
LOCAL_MODULE := Velvet
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE := true
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_OVERRIDES_PACKAGES := QuickSearchBox
ifeq ($(my_src_arch),arm)
  LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
else
  LOCAL_SRC_FILES := $(LOCAL_MODULE)_$(my_src_arch).apk
endif
LOCAL_REQUIRED_MODULES := \
    en-US/action.pumpkin \
    en-US/c_fst \
    en-US/class_normalizer.mfar \
    en-US/CLG.prewalk.fst \
    en-US/commands.abnf \
    en-US/compile_grammar.config \
    en-US/config.pumpkin \
    en-US/contacts.abnf \
    en-US/CONTACTS.fst \
    en-US/CONTACTS.syms \
    en-US/dict \
    en-US/dictation.config \
    en-US/dist_belief \
    en-US/dnn \
    en-US/endpointer_dictation.config \
    en-US/endpointer_model.mmap \
    en-US/endpointer_voicesearch.config \
    en-US/g2p.data \
    en-US/g2p_fst \
    en-US/grammar.config \
    en-US/graphemes.syms \
    en-US/hmmlist \
    en-US/hmm_symbols \
    en-US/input_mean_std_dev \
    en-US/lexicon.U.fst \
    en-US/lstm_model.uint8.data \
    en-US/magic_mic.config \
    en-US/metadata \
    en-US/normalizer.mfar \
    en-US/norm_fst \
    en-US/offensive_word_normalizer.mfar \
    en-US/phonelist \
    en-US/phonelist.syms \
    en-US/phonemes.syms \
    en-US/rescoring.fst.louds \
    en-US/semantics.pumpkin \
    en-US/voice_actions_compiler.config \
    en-US/voice_actions.config \
    en-US/wordlist.syms
#LOCAL_PREBUILT_JNI_LIBS :=
LOCAL_MODULE_TARGET_ARCH := $(my_src_arch)
include $(BUILD_PREBUILT)

include $(LOCAL_PATH)/OfflineVoiceRecognitionLanguagePacks/Android.mk
