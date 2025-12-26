#!/bin/sh

enable_gpu_plugin() {
	sed -i -e 's/without_gpu_nvidia:0/without_gpu_nvidia:1/' \
	-e 's/BuildRequires: cuda-nvml-dev-10-1/BuildRequires: cuda-nvml-devel-12-4/' \
	-e '/%{?_with_gpu_nvidia} \\/a\
		--with-cuda=/usr/local/cuda \\' \
	-e 's/write_gpu_nvidia/gpu_nvidia/g' \
	contrib/redhat/collectd.spec
}

enable_gpu_plugin_on_el10() {
	sed -i -e 's/without_gpu_nvidia:0/without_gpu_nvidia:1/' \
	-e 's/BuildRequires: cuda-nvml-dev-10-1/BuildRequires: cuda-nvml-devel-13-1/' \
	-e '/%{?_with_gpu_nvidia} \\/a\
		--with-cuda=/usr/local/cuda \\' \
	-e 's/write_gpu_nvidia/gpu_nvidia/g' \
	contrib/redhat/collectd.spec

	sed -i -e '/unsigned int core_temp;/i\
	#if NVML_API_VERSION >= 13\
	    nvmlTemperature_t core_temp;\
	    memset(&core_temp, 0, sizeof(core_temp));\
	    core_temp.version = nvmlTemperature_v1;\
	    core_temp.sensorType = NVML_TEMPERATURE_GPU;\
	    TRYOPT(nvmlDeviceGetTemperatureV(dev, &core_temp))\
	    if (nv_status == NVML_SUCCESS)\
	      nvml_submit_gauge(ix, dev_name, "temperature", "core", core_temp.temperature);\
	#else' \
	-e '/nvml_submit_gauge(ix, dev_name, "temperature", "core", core_temp);/a\
	#endif' \
	src/gpu_nvidia.c
}

disable_deprecated_plugins_on_el10() {
	sed -i -e 's/without_dbi:1/without_dbi:0/' \
	-e 's/without_ceph:1/without_ceph:0/' \
	-e 's/without_connectivity:1/without_connectivity:0/' \
	-e 's/without_curl_json:1/without_curl_json:0/' \
	-e 's/without_log_logstash:1/without_log_logstash:0/' \
	-e 's/without_ovs_events:1/without_ovs_events:0/' \
	-e 's/without_ovs_stats:1/without_ovs_stats:0/' \
	-e 's/without_procevent:1/without_procevent:0/' \
	-e 's/without_sysevent:1/without_sysevent:0/' \
	-e 's/without_write_stackdriver:1/without_write_stackdriver:0/' \
	-e 's/without_iptables:1/without_iptables:0/' \
	contrib/redhat/collectd.spec
}

enable_gpu_plugin_on_ubuntu() {
	sed -i -e 's/confflags += --disable-gpu_nvidia/confflags += --with-cuda=\/usr\/local\/cuda/' debian/rules
}

if [ "$1" = "el8" -o "$1" = "el9" ]; then
	enable_gpu_plugin
	./build.sh && ./configure && make rpms
elif [ "$1" = "el10" ]; then
	enable_gpu_plugin_on_el10
	disable_deprecated_plugins_on_el10
	./build.sh && ./configure && make rpms
elif [ "$1" = "ubuntu" ]; then
	enable_gpu_plugin_on_ubuntu
	tag_name=$(git describe --tags)
	src_prefix="collectd_${tag_name#*-}"
	tar Jcvf ../${src_prefix}.orig.tar.xz .
	debuild -us -uc
fi
