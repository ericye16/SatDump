#include "dsp_sample_source.h"
#include "core/plugin.h"

namespace dsp
{
    std::map<std::string, RegisteredSource> dsp_sources_registry;

    std::vector<SourceDescriptor> getAllAvailableSources()
    {
        std::vector<SourceDescriptor> all_sources;

        for (std::pair<std::string, RegisteredSource> source : dsp_sources_registry)
        {
            std::vector<SourceDescriptor> devices = source.second.getSources();
            all_sources.insert(all_sources.end(), devices.begin(), devices.end());
        }

        return all_sources;
    }

    std::shared_ptr<DSPSampleSource> getSourceFromDescriptor(SourceDescriptor descriptor)
    {
        for (std::pair<std::string, RegisteredSource> source : dsp_sources_registry)
            if (descriptor.source_type == source.first)
                return source.second.getInstance(descriptor);
        throw std::runtime_error("Could not find a handler for device " + descriptor.name);
    }

    void registerAllSources()
    {
        // dsp_sources_registry.insert({AirspySource::getID(), {AirspySource::getInstance, AirspySource::getAvailableSources}});

        // Plugin Sources
        satdump::eventBus->fire_event<RegisterDSPSampleSourcesEvent>({dsp_sources_registry});
    }
}