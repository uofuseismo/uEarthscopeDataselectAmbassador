#ifndef METRICS_HPP
#define METRICS_HPP
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#ifdef WITH_OTLP_GRPC
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#endif
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#include "utilities/otelOptions.hpp"
#include "httpServer/programOptions.hpp"

namespace
{

bool metricsInitialized{false};

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    serverSuccessCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    clientErrorCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    earthscopeUnauthorizedCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    earthscope204ResponsesCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    serverErrorCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    bytesDownloadedCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    earthscopeRequestsCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    earthscopeRetriesCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    rateLimitedCounter{nullptr};
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    cache204HitCounter{nullptr};
opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>>
    routePerformanceHistogram{nullptr};


void initializeRouteHistogram(
    opentelemetry::sdk::metrics::MeterProvider *providerInstance)
{
    auto histogramInstrumentSelector
        = opentelemetry::sdk::metrics::InstrumentSelectorFactory::Create(
             opentelemetry::sdk::metrics::InstrumentType::kHistogram,
             "uearthscope-dataselect-ambassador.route.duration",
             "s");
    auto histogramMeterSelector
        = opentelemetry::sdk::metrics::MeterSelectorFactory::Create(
             "route_duration_histogram",
             "1.2.0",
             "https://opentelemetry.io/schemas/1.2.0");
    auto histogramAggregationConfig
        = std::make_shared<opentelemetry::sdk::metrics::HistogramAggregationConfig> ();
    histogramAggregationConfig->boundaries_
        = std::vector<double> {
                               0.000,
                               0.010,
                               0.050,
                               0.100,
                               0.500,
                               1.000,
                               2.500,
                               5.000,
                               10.00,
                               60.00,
                               120.0,
                               300.0,
                               };
    // The view RENAMES the metric on export, so this has to stay the
    // instrument's name.
    auto histogramView
        = opentelemetry::sdk::metrics::ViewFactory::Create(
             "uearthscope-dataselect-ambassador.route.duration",
             "Time required to for a route to succcessfully complete.",
             opentelemetry::sdk::metrics::AggregationType::kHistogram,
             histogramAggregationConfig);

    providerInstance->AddView(std::move(histogramInstrumentSelector),
                      std::move(histogramMeterSelector),
                      std::move(histogramView));
}


void initializeHTTP(
    const bool exportMetrics,
    const auto &otelHTTPMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpHttpMetricExporterOptions exporterOptions;
    exporterOptions.url = otelHTTPMetricsOptions.url
                        + otelHTTPMetricsOptions.suffix;
    //exporterOptions.console_debug = debug != "" && debug != "0" && debug != "no";
    exporterOptions.content_type
        = otel::exporter::otlp::HttpRequestContentType::kBinary;

    auto exporter
        = otel::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
             exporterOptions);
    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelHTTPMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelHTTPMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    ::initializeRouteHistogram(metricsProvider.get());

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}

#ifdef WITH_OTLP_GRPC
void initializeGRPC(
    const bool exportMetrics,
    const auto &otelGRPCMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpGrpcMetricExporterOptions exporterOptions;
    exporterOptions.endpoint = otelGRPCMetricsOptions.url;
    exporterOptions.use_ssl_credentials = false;
    if (!otelGRPCMetricsOptions.certificatePath.empty())
    {
        exporterOptions.use_ssl_credentials = true;
        exporterOptions.ssl_credentials_cacert_path
           = otelGRPCMetricsOptions.certificatePath;
    }
    auto exporter
        = otel::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
             exporterOptions);

    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelGRPCMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelGRPCMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    ::initializeRouteHistogram(metricsProvider.get());

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}
#endif

/// Send 200s for routes
void observeServerSuccessCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        auto map = instance.getSuccessCounters();
        for (const auto &item : map)
        {
            auto key = item.first;
            auto value = item.second;
            std::map<std::string, std::string>
                attribute{ {"route", item.first} };
            observer->Observe(value, attribute);
        }   
    }   
}

// Earthscope request counts
void observeEarthscopeRequestCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getEarthscopeRequestsCount());
    }
}

// Bad earthscope credentials
void observeEarthscopeUnauthorizedCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getClientUnauthorizedCounts());
    }   
}

// Earthscope 204 counts
void observeEarthscope204ResponseCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <   
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >   
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <   
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getEarthscope204ResponseCounts());
    }
}
 
// Earthscope retries counts
void observeEarthscopeRetriesCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getEarthscopeRetriesCount());
    }
}

// Earthscope data downloaded
void observeEarthscopeBytesDownloaded(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getBytesDownloadedCount());
    }
}

// Rate limited
void observeRateLimitedRequestsCounts(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.getRateLimitedRequestsCount());
    }   
}

// 204 cache hit rejects
void observe204CacheHits(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{   
    if (opentelemetry::nostd::holds_alternative
        <    
            opentelemetry::nostd::shared_ptr
            <                   
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <                       
            opentelemetry::nostd::shared_ptr
            < 
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        // N.B. these should all be noexcepts
        auto &instance
            = UEarthscopeDataselectAmbassador::Metrics::
                 MetricsSingleton::getInstance();
        observer->Observe(instance.get204CacheHitCount());
    }
}

auto makeInt64Counter( 
    auto &meter, const char *name, const char *description,
    const char *unit,
    opentelemetry::metrics::ObservableCallbackPtr callback)
{
    auto counter = meter->CreateInt64ObservableCounter(name, description, unit);
    counter->AddCallback(callback, nullptr);
    return counter;
}

}

namespace UEarthscopeDataselectAmbassador::Metrics
{

void initialize(
    // NOLINTNEXTLINE(misc-include-cleaner)
    const ::ProgramOptions &options)
{
    if (options.exportMetricsWithHTTP)
    {
        return ::initializeHTTP(options.exportMetrics,
                                options.otelHTTPMetricsOptions);
    }
    else
    {
#ifdef WITH_OTLP_GRPC
        return ::initializeGRPC(options.exportMetrics,
                                options.otelGRPCMetricsOptions);
#else
        throw std::runtime_error("Recompile with Conan and OTLP_GRPC");
#endif
    }
}

/*
void recordEarthscopePostRequestDuration(
    const std::chrono::nanoseconds &duration)
{
    if (!metricsInitialized){return;}
    auto durationSeconds = static_cast<double> (duration.count())*1.e-9;
    std::map<std::string, std::string> histogramKey{ {"request_type", "post"} };
    earthscopePerformanceHistogram->Record(durationSeconds, histogramKey);
}

void recordEarthscopeGetRequestDuration(
    const std::chrono::nanoseconds &duration)
{
    if (!metricsInitialized){return;}
    auto durationSeconds = static_cast<double> (duration.count())*1.e-9;
    std::map<std::string, std::string> histogramKey{ {"request_type", "get"} };
    earthscopePerformanceHistogram->Record(durationSeconds, histogramKey);
}
*/

void recordClientRequestDuration(const std::string &route,
                                 const std::chrono::nanoseconds &duration)
{
    if (!metricsInitialized){return;}
    auto durationSeconds = static_cast<double> (duration.count())*1.e-9;
    std::map<std::string, std::string> histogramKey{ {"route", route} };
    routePerformanceHistogram->Record(durationSeconds, histogramKey);
}


void cleanup()
{
    if (metricsInitialized)
    {   
        const std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
        opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
    }
    metricsInitialized = false;
}

void createMeters(const ::ProgramOptions &options)
{
    if (!options.exportMetrics){return;}

    auto provider
        = opentelemetry::metrics::Provider::GetMeterProvider();
    if (provider == nullptr)
    {
        throw std::runtime_error("Provider is null");
    }
    auto meter = provider->GetMeter(options.applicationName, "1.2.0");
    if (meter == nullptr){std::runtime_error("Meter is null");}

    // 200 counts (all routes)
    serverSuccessCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.server.responses.success",
             "Number of 200 responses to user.",
             "",
             ::observeServerSuccessCounts);

    // Rate limited counts
    rateLimitedCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.server.rate_limited_requests",
             "Number of requests rejected by the rate limiter",
             "{request}",
             ::observeRateLimitedRequestsCounts);

    // Earthscope request counts
    earthscopeRequestsCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.client.requests",
             "Number of requests made to Earthscope",
             "{request}",
             ::observeEarthscopeRequestCounts);

    // Passed wrong creds to earthscope
    earthscopeUnauthorizedCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.client.unauthorized",
             "Number of requests rejected by Earthscope because of invalid credentials",
             "{request}",
             ::observeEarthscopeUnauthorizedCounts);

    // 204 responses from earthscope
    earthscope204ResponsesCounter
       = ::makeInt64Counter(
            meter,
            "seismic_data.earthscope.client.no_data",
            "Number of 204 responses returned from Earthscope",
            "{response}",
            ::observeEarthscope204ResponseCounts); 

    // Earthscope retries counts
    earthscopeRetriesCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.client.retried_requests",
             "Number of retried requests made to Earthscope",
             "{request}",
             ::observeEarthscopeRetriesCounts);

    // Data download
    bytesDownloadedCounter
        = ::makeInt64Counter(
             meter,
             "seismic_data.earthscope.client.downloaded",
             "Amount of data downloaded from Earthscope's dataselect in bytes",
             "By",
             ::observeEarthscopeBytesDownloaded);

    // 204 cache hits
    cache204HitCounter
        = ::makeInt64Counter( 
             meter,
             "seismic_data.earthscope.server.no_data_cache_hit_rejections",
             "Amount of requests rejected because they are duplicates of previous 204 requests",
             "{request}",
             ::observe204CacheHits);
/*
    // 200 counts from earthscope
    clientSuccessCounter
        = meter->CreateInt64ObservableCounter(
             "seismic_data.earthscope.client.responses.success",
             "Number of 200 responses to ambassador from Earthscope.",
             "");
    clientSuccessCounter->AddCallback(::observeClientSuccessCounts, nullptr);
*/

    auto routeHistogramMeter
        = provider->GetMeter("route_duration_histogram", "1.2.0");
    routePerformanceHistogram 
        = routeHistogramMeter->CreateDoubleHistogram(
            "seismic_data.earthscope.server.route.duration",
            "Time required for a route to succcessfully complete",
            "s");
}

}

#endif
