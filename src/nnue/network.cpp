/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)
  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "network.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <optional>
#include <vector>

#include "../memory.h"
#include "../misc.h"
#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_misc.h"


namespace Stockfish::Eval::NNUE {

namespace Detail {

// Read evaluation function parameters
template<typename T>
bool read_parameters(std::istream& stream, T& reference) {

    std::uint32_t header;
    header = read_little_endian<std::uint32_t>(stream);
    if (!stream || header != T::get_hash_value())
        return false;
    return reference.read_parameters(stream);
}

// Write evaluation function parameters
template<typename T>
bool write_parameters(std::ostream& stream, const T& reference) {

    write_little_endian<std::uint32_t>(stream, T::get_hash_value());
    return reference.write_parameters(stream);
}

}  // namespace Detail

Network::Network(const Network& other) :
    evalFile(other.evalFile) {

    if (other.featureTransformer)
        featureTransformer = make_unique_large_page<FeatureTransformer>(*other.featureTransformer);

    network = make_unique_aligned<NetworkArchitecture[]>(LayerStacks);

    if (!other.network)
        return;
    for (std::size_t i = 0; i < LayerStacks; ++i)
        network[i] = other.network[i];
}

Network& Network::operator=(const Network& other) {
    evalFile = other.evalFile;

    featureTransformer = make_unique_large_page<FeatureTransformer>(*other.featureTransformer);

    network = make_unique_aligned<NetworkArchitecture[]>(LayerStacks);

    if (!other.network)
        return *this;
    for (std::size_t i = 0; i < LayerStacks; ++i)
        network[i] = other.network[i];

    return *this;
}

void Network::load(const std::string& rootDirectory, std::string evalfilePath) {
#if defined(DEFAULT_NNUE_DIRECTORY)
    std::vector<std::string> dirs = {"", rootDirectory, stringify(DEFAULT_NNUE_DIRECTORY)};
#else
    std::vector<std::string> dirs = {"", rootDirectory};
#endif

    if (evalfilePath.empty())
        evalfilePath = evalFile.defaultName;

    for (const auto& directory : dirs)
    {
        if (evalFile.current != evalfilePath)
        {
            load_user_net(directory, evalfilePath);
        }
    }
}


bool Network::save(const std::optional<std::string>& filename) const {
    std::string actualFilename;
    std::string msg;

    if (filename.has_value())
        actualFilename = filename.value();
    else
    {
        if (evalFile.current != evalFile.defaultName)
        {
            msg = "Failed to export a net. "
                  "A non-embedded net can only be saved if the filename is specified";

            sync_cout << msg << sync_endl;
            return false;
        }

        actualFilename = evalFile.defaultName;
    }

    std::ofstream stream(actualFilename, std::ios_base::binary);
    bool          saved = save(stream, evalFile.current, evalFile.netDescription);

    msg = saved ? "Network saved successfully to " + actualFilename : "Failed to export a net";

    sync_cout << msg << sync_endl;
    return saved;
}


NetworkOutput Network::evaluate(const Position& pos, AccumulatorCaches::Cache* cache) const {
    // We manually align the arrays on the stack because with gcc < 9.3
    // overaligning stack variables with alignas() doesn't work correctly.

    constexpr uint64_t alignment = CacheLineSize;

#if defined(ALIGNAS_ON_STACK_VARIABLES_BROKEN)
    TransformedFeatureType
      transformedFeaturesUnaligned[FeatureTransformer::BufferSize
                                   + alignment / sizeof(TransformedFeatureType)];

    auto* transformedFeatures = align_ptr_up<alignment>(&transformedFeaturesUnaligned[0]);
#else
    alignas(alignment) TransformedFeatureType transformedFeatures[FeatureTransformer::BufferSize];
#endif

    ASSERT_ALIGNED(transformedFeatures, alignment);

    const int  bucket     = FeatureSet::make_layer_stack_bucket(pos);
    const auto psqt       = featureTransformer->transform(pos, cache, transformedFeatures, bucket);
    const auto positional = network[bucket].propagate(transformedFeatures);

    return {static_cast<Value>(psqt / OutputScale), static_cast<Value>(positional / OutputScale)};
}


void Network::verify(std::string evalfilePath) const {
    if (evalfilePath.empty())
        evalfilePath = evalFile.defaultName;

    if (evalFile.current != evalfilePath)
    {
        std::string msg1 =
          "Network evaluation parameters compatible with the engine must be available.";
        std::string msg2 = "The network file " + evalfilePath + " was not loaded successfully.";
        std::string msg3 = "The UCI option EvalFile might need to specify the full path, "
                           "including the directory name, to the network file.";
        std::string msg4 =
          "The default net can be downloaded from: "
          "https://github.com/official-pikafish/Networks/releases/download/master-net/"
          + evalFile.defaultName;
        std::string msg5 = "The engine will be terminated now.";

        sync_cout << "info string ERROR: " << msg1 << sync_endl;
        sync_cout << "info string ERROR: " << msg2 << sync_endl;
        sync_cout << "info string ERROR: " << msg3 << sync_endl;
        sync_cout << "info string ERROR: " << msg4 << sync_endl;
        sync_cout << "info string ERROR: " << msg5 << sync_endl;
        exit(EXIT_FAILURE);
    }

    size_t size = sizeof(*featureTransformer) + sizeof(NetworkArchitecture) * LayerStacks;
    sync_cout << "info string NNUE evaluation using " << evalfilePath << " ("
              << size / (1024 * 1024) << "MiB, (" << featureTransformer->InputDimensions << ", "
              << TransformedFeatureDimensions << ", " << NetworkArchitecture::FC_0_OUTPUTS << ", "
              << NetworkArchitecture::FC_1_OUTPUTS << ", 1))" << sync_endl;
}


void Network::hint_common_access(const Position& pos, AccumulatorCaches::Cache* cache) const {
    featureTransformer->hint_common_access(pos, cache);
}


NnueEvalTrace Network::trace_evaluate(const Position& pos, AccumulatorCaches::Cache* cache) const {
    // We manually align the arrays on the stack because with gcc < 9.3
    // overaligning stack variables with alignas() doesn't work correctly.
    constexpr uint64_t alignment = CacheLineSize;

#if defined(ALIGNAS_ON_STACK_VARIABLES_BROKEN)
    TransformedFeatureType
      transformedFeaturesUnaligned[FeatureTransformer::BufferSize
                                   + alignment / sizeof(TransformedFeatureType)];

    auto* transformedFeatures = align_ptr_up<alignment>(&transformedFeaturesUnaligned[0]);
#else
    alignas(alignment) TransformedFeatureType transformedFeatures[FeatureTransformer::BufferSize];
#endif

    ASSERT_ALIGNED(transformedFeatures, alignment);

    NnueEvalTrace t{};
    t.correctBucket = FeatureSet::make_layer_stack_bucket(pos);
    for (IndexType bucket = 0; bucket < LayerStacks; ++bucket)
    {
        const auto materialist =
          featureTransformer->transform(pos, cache, transformedFeatures, bucket);
        const auto positional = network[bucket].propagate(transformedFeatures);

        t.psqt[bucket]       = static_cast<Value>(materialist / OutputScale);
        t.positional[bucket] = static_cast<Value>(positional / OutputScale);
    }

    return t;
}


void Network::load_user_net(const std::string& dir, const std::string& evalfilePath) {
    std::stringstream sstream     = read_zipped_nnue(dir + evalfilePath);
    auto              description = load(sstream);

    if (!description.has_value())
    {
        std::ifstream stream(dir + evalfilePath, std::ios::binary);
        description = load(stream);
    }

    if (description.has_value())
    {
        evalFile.current        = evalfilePath;
        evalFile.netDescription = description.value();
    }
}


void Network::initialize() {
    featureTransformer = make_unique_large_page<FeatureTransformer>();
    network            = make_unique_aligned<NetworkArchitecture[]>(LayerStacks);
}


bool Network::save(std::ostream&      stream,
                   const std::string& name,
                   const std::string& netDescription) const {
    if (name.empty() || name == "None")
        return false;

    return write_parameters(stream, netDescription);
}


std::optional<std::string> Network::load(std::istream& stream) {
    initialize();
    std::string description;

    return read_parameters(stream, description) ? std::make_optional(description) : std::nullopt;
}


// Read network header
bool Network::read_header(std::istream& stream, std::uint32_t* hashValue, std::string* desc) const {
    std::uint32_t version, size;

    version    = read_little_endian<std::uint32_t>(stream);
    *hashValue = read_little_endian<std::uint32_t>(stream);
    size       = read_little_endian<std::uint32_t>(stream);
    if (!stream || version != Version)
        return false;
    desc->resize(size);
    stream.read(&(*desc)[0], size);
    return !stream.fail();
}


// Write network header
bool Network::write_header(std::ostream&      stream,
                           std::uint32_t      hashValue,
                           const std::string& desc) const {
    write_little_endian<std::uint32_t>(stream, Version);
    write_little_endian<std::uint32_t>(stream, hashValue);
    write_little_endian<std::uint32_t>(stream, std::uint32_t(desc.size()));
    stream.write(&desc[0], desc.size());
    return !stream.fail();
}


bool Network::read_parameters(std::istream& stream, std::string& netDescription) const {
    std::uint32_t hashValue;
    if (!read_header(stream, &hashValue, &netDescription))
        return false;
    if (hashValue != Network::hash)
        return false;
    if (!Detail::read_parameters(stream, *featureTransformer))
        return false;
    for (std::size_t i = 0; i < LayerStacks; ++i)
    {
        if (!Detail::read_parameters(stream, network[i]))
            return false;
    }
    return stream && stream.peek() == std::ios::traits_type::eof();
}


bool Network::write_parameters(std::ostream& stream, const std::string& netDescription) const {
    if (!write_header(stream, Network::hash, netDescription))
        return false;
    if (!Detail::write_parameters(stream, *featureTransformer))
        return false;
    for (std::size_t i = 0; i < LayerStacks; ++i)
    {
        if (!Detail::write_parameters(stream, network[i]))
            return false;
    }
    return bool(stream);
}

}  // namespace Stockfish::Eval::NNUE
