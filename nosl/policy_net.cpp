// See policy_net.h. Mirrors python/nosl/model.py ActorCritic.forward for one
// observation; every step below names the line of that forward it follows.
#include "nosl/policy_net.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace sts2::nosl {

namespace {

// Format 1 is the features-1 model; format 2 adds a ninth header field
// (features: 2, 3 or 4) and, after the format-1 tensors, the card table and its
// layers (features >= 2), route layer (>= 3) and deck-summary layer (4).
// Format 3 adds (features, power_context) after the eight-field header and
// appends the owner-pooling affine layer after the feature tensors.
constexpr int32_t kMagic = 0x4D534F4E, kFormat = 1, kFormatFeatures = 2, kFormatPowerContext = 3;
// Format 4 adds (features, power_context, action_interaction=1), then a
// bias-free action interaction projection after all earlier tensors.
constexpr int32_t kFormatActionInteraction = 4;
// model.py: CARD_LEVELS, CARD_NUMBERS, CARD_FLAGS, CARD_POWERS, TOKEN_ENEMY, TOKEN_SHOP.
constexpr int32_t kCardLevels = 2, kCardNumbers = 14, kCardFlags = 42, kCardPowers = 2;
constexpr int32_t kDeckNumbers = 2 * kCardNumbers + kCardFlags + 2;
// model.py: VOCAB, DECISION_KINDS, ACTION_KINDS, ENCOUNTERS, ID_COLUMNS, G_ID_LIKE, MISC_EVENT.
constexpr int32_t kVocab[7] = {18, 597, 300, 66, 121, 266, 1280};  // kind card relic potion monster power misc
constexpr int32_t kIdColumnTable[8] = {0, 1, 2, 3, 4, 5, 6, 6};
constexpr int32_t kDecisionKinds = 10, kActionKinds = 14, kEncounters = 91, kMiscEvent = 128;
constexpr size_t kIdLike[] = {kGSchema, kGDecision, kGCharacter, kGBoss, kGRoomType, kGChoicePrompt, kGChoiceSource, kGEvent};
constexpr size_t kNumbers = kTokenWidth - 8;
// model.py: MAP_ROWS, MAP_COLS, MAP_ELITE.
constexpr int32_t kMapRows = 24, kMapCols = 8, kMapElite = 1 + 6;

struct Tensor {
    std::vector<int32_t> dims;
    std::vector<float> data;
    const float* row(int32_t i) const { return data.data() + static_cast<size_t>(i) * static_cast<size_t>(dims[1]); }
};

float LogScale(float x) { return (x > 0 ? 1.0f : x < 0 ? -1.0f : 0.0f) * std::log1p(std::fabs(x)) / 3.0f; }
float ClipScale(float x) { return std::clamp(x, -20.0f, 20.0f) / 10.0f; }

// model.py ActorCritic.elites_ahead: for every map cell, the fewest elites on
// any path from it to the end of the act, itself included. Read from the
// map-node tokens (type, col, row, columns of the children in the next row).
std::vector<int32_t> ElitesAhead(const Observation& obs) {
    const size_t cells = static_cast<size_t>(kMapRows) * kMapCols;
    std::vector<int32_t> elite(cells, 0), exists(cells, 0), children(cells, 0), cost(cells, 0);
    for (size_t t = 0; t < obs.token_count(); ++t) {
        const int32_t* token = &obs.tokens[t * kTokenWidth];
        if (token[kColType] != static_cast<int32_t>(TokenType::MapNode)) continue;
        const size_t cell = static_cast<size_t>(std::clamp(token[kColN1], 0, kMapRows - 1)) * kMapCols +
                            static_cast<size_t>(std::clamp(token[kColN0], 0, kMapCols - 1));
        elite[cell] = token[kColMiscA] == kMapElite;
        exists[cell] = 1;
        children[cell] = token[kColN4];
    }
    for (int32_t row = kMapRows - 1; row >= 0; --row) {
        for (int32_t col = 0; col < kMapCols; ++col) {
            const size_t cell = static_cast<size_t>(row) * kMapCols + static_cast<size_t>(col);
            int32_t onward = -1;
            if (row + 1 < kMapRows)
                for (int32_t next = 0; next < kMapCols; ++next) {
                    const size_t to = static_cast<size_t>(row + 1) * kMapCols + static_cast<size_t>(next);
                    if ((children[cell] >> next & 1) && exists[to] && (onward < 0 || cost[to] < onward)) onward = cost[to];
                }
            cost[cell] = elite[cell] + std::max(onward, 0);
        }
    }
    return cost;
}

// out = W x + b, W is [rows x cols] (torch.nn.Linear).
void Linear(const Tensor& w, const Tensor& b, const float* x, float* out) {
    const int32_t rows = w.dims[0], cols = w.dims[1];
    for (int32_t r = 0; r < rows; ++r) {
        const float* wr = w.data.data() + static_cast<size_t>(r) * static_cast<size_t>(cols);
        float sum = b.data[static_cast<size_t>(r)];
        for (int32_t c = 0; c < cols; ++c) sum += wr[c] * x[c];
        out[r] = sum;
    }
}

void LayerNorm(const Tensor& w, const Tensor& b, float* x, int32_t n) {
    float mean = 0;
    for (int32_t i = 0; i < n; ++i) mean += x[i];
    mean /= static_cast<float>(n);
    float var = 0;
    for (int32_t i = 0; i < n; ++i) var += (x[i] - mean) * (x[i] - mean);
    const float scale = 1.0f / std::sqrt(var / static_cast<float>(n) + 1e-5f);
    for (int32_t i = 0; i < n; ++i) x[i] = (x[i] - mean) * scale * w.data[static_cast<size_t>(i)] + b.data[static_cast<size_t>(i)];
}

}  // namespace

struct PolicyNet::Weights {
    int32_t width = 0, layers = 0, heads = 0, feedforward = 0;
    Tensor embed[7];
    Tensor token_w, token_b, token_norm_w, token_norm_b;
    Tensor decision, boss, global_w, global_b, global_norm_w, global_norm_b;
    struct Layer { Tensor in_w, in_b, out_w, out_b, l1_w, l1_b, l2_w, l2_b, n1_w, n1_b, n2_w, n2_b; };
    std::vector<Layer> layer;
    Tensor final_w, final_b, action_kind, target_w, target_b, number_w, number_b;
    Tensor p0_w, p0_b, p2_w, p2_b, v0_w, v0_b, v2_w, v2_b;
    int32_t features = 1;
    Tensor card_numbers, card_flags, card_powers, card_w, card_b, extra_w, extra_b;  // features >= 2
    Tensor route_w, route_b;                                                         // features >= 3
    Tensor deck_w, deck_b;                                                           // features >= 4
    int32_t power_context = 0;
    Tensor power_w, power_b;
    int32_t action_interaction = 0;
    Tensor interaction_w, interaction_zero_b;
};

PolicyNet::PolicyNet(std::unique_ptr<Weights> weights) : w_(std::move(weights)) {}
PolicyNet::~PolicyNet() = default;

std::unique_ptr<PolicyNet> PolicyNet::Load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read policy weights " + path);
    auto read_ints = [&](int32_t* out, size_t count) {
        in.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count * sizeof(int32_t)));
        if (!in) throw std::runtime_error("policy weights file is truncated");
    };
    int32_t header[8];
    read_ints(header, 8);
    if (header[0] != kMagic || (header[1] != kFormat && header[1] != kFormatFeatures
        && header[1] != kFormatPowerContext && header[1] != kFormatActionInteraction))
        throw std::runtime_error("not a policy weights file of format 1, 2, 3 or 4");
    if (header[2] != kObsSchemaVersion) throw std::runtime_error("policy weights were trained on another observation schema");
    auto w = std::make_unique<Weights>();
    if (header[1] == kFormatFeatures || header[1] == kFormatPowerContext || header[1] == kFormatActionInteraction) {
        read_ints(&w->features, 1);
        if (w->features < (header[1] == kFormatFeatures ? 2 : 1) || w->features > 4)
            throw std::runtime_error("policy weights use model features this build does not know");
    }
    if (header[1] == kFormatPowerContext || header[1] == kFormatActionInteraction) {
        read_ints(&w->power_context, 1);
        if (w->power_context != 1 && !(header[1] == kFormatActionInteraction && w->power_context == 0))
            throw std::runtime_error("policy weights use an unknown power-context setting");
    }
    if (header[1] == kFormatActionInteraction) {
        read_ints(&w->action_interaction, 1);
        if (w->action_interaction != 1) throw std::runtime_error("policy weights use an unknown action-interaction setting");
    }
    w->width = header[3]; w->layers = header[4]; w->heads = header[5]; w->feedforward = header[6];
    if (w->width <= 0 || w->heads <= 0 || w->width % w->heads != 0 || w->layers < 0)
        throw std::runtime_error("policy weights have an impossible shape");
    int32_t remaining = header[7];
    auto read = [&](Tensor& t, std::vector<int32_t> expected) {
        if (remaining-- <= 0) throw std::runtime_error("policy weights file has too few tensors");
        int32_t rank = 0;
        read_ints(&rank, 1);
        if (rank != static_cast<int32_t>(expected.size())) throw std::runtime_error("policy tensor has the wrong rank");
        t.dims.resize(expected.size());
        read_ints(t.dims.data(), expected.size());
        if (t.dims != expected) throw std::runtime_error("policy tensor has the wrong shape");
        size_t count = 1;
        for (int32_t d : expected) count *= static_cast<size_t>(d);
        t.data.resize(count);
        in.read(reinterpret_cast<char*>(t.data.data()), static_cast<std::streamsize>(count * sizeof(float)));
        if (!in) throw std::runtime_error("policy weights file is truncated");
    };
    const int32_t d = w->width, f = w->feedforward;
    for (int i = 0; i < 7; ++i) read(w->embed[i], {kVocab[i], d});
    read(w->token_w, {d, static_cast<int32_t>(2 * kNumbers)}); read(w->token_b, {d});
    read(w->token_norm_w, {d}); read(w->token_norm_b, {d});
    read(w->decision, {kDecisionKinds, d}); read(w->boss, {kEncounters, d});
    read(w->global_w, {d, static_cast<int32_t>(2 * kGlobalWidth)}); read(w->global_b, {d});
    read(w->global_norm_w, {d}); read(w->global_norm_b, {d});
    w->layer.resize(static_cast<size_t>(w->layers));
    for (auto& l : w->layer) {
        read(l.in_w, {3 * d, d}); read(l.in_b, {3 * d}); read(l.out_w, {d, d}); read(l.out_b, {d});
        read(l.l1_w, {f, d}); read(l.l1_b, {f}); read(l.l2_w, {d, f}); read(l.l2_b, {d});
        read(l.n1_w, {d}); read(l.n1_b, {d}); read(l.n2_w, {d}); read(l.n2_b, {d});
    }
    read(w->final_w, {d}); read(w->final_b, {d});
    read(w->action_kind, {kActionKinds, d});
    read(w->target_w, {d, d}); read(w->target_b, {d});
    read(w->number_w, {d, 2}); read(w->number_b, {d});
    read(w->p0_w, {d, 2 * d}); read(w->p0_b, {d}); read(w->p2_w, {1, d}); read(w->p2_b, {1});
    read(w->v0_w, {d, d}); read(w->v0_b, {d}); read(w->v2_w, {1, d}); read(w->v2_b, {1});
    if (w->features >= 2) {
        read(w->card_numbers, {kVocab[1], kCardLevels, kCardNumbers});
        read(w->card_flags, {kVocab[1], kCardLevels, kCardFlags});
        read(w->card_powers, {kVocab[1], kCardLevels, kCardPowers});
        read(w->card_w, {d, 2 * kCardNumbers + kCardFlags}); read(w->card_b, {d});
        read(w->extra_w, {d, 6}); read(w->extra_b, {d});
    }
    if (w->features >= 3) { read(w->route_w, {d, 4}); read(w->route_b, {d}); }
    if (w->features >= 4) { read(w->deck_w, {d, 2 * kDeckNumbers}); read(w->deck_b, {d}); }
    if (w->power_context) { read(w->power_w, {d, 2 * d}); read(w->power_b, {d}); }
    if (w->action_interaction) {
        read(w->interaction_w, {d, d});
        w->interaction_zero_b.data.resize(static_cast<size_t>(d), 0.0f);
    }
    if (remaining != 0) throw std::runtime_error("policy weights file has extra tensors");
    return std::unique_ptr<PolicyNet>(new PolicyNet(std::move(w)));
}

void PolicyNet::Forward(const Observation& obs, std::vector<float>& logits, float& value) const {
    const Weights& w = *w_;
    const int32_t d = w.width;
    const size_t D = static_cast<size_t>(d);
    const size_t tokens = obs.token_count(), actions = obs.action_count(), length = tokens + 1;
    std::vector<float> x(length * D, 0.0f);
    // model.py power_summary: shown power embeddings weighted by the two
    // numeric amount scales, grouped by the public owner ordinal.
    std::vector<float> power_pool;
    std::vector<uint8_t> power_present;
    if (w.power_context) {
        power_pool.assign((tokens + 1) * 2 * D, 0.0f);
        power_present.assign(tokens + 1, 0);
        for (size_t t = 0; t < tokens; ++t) {
            const int32_t* token = &obs.tokens[t * kTokenWidth];
            const int32_t owner = token[kColN1];
            if (token[kColType] != static_cast<int32_t>(TokenType::Power) || token[kColPower] <= 0 ||
                owner < 0 || static_cast<size_t>(owner) > tokens) continue;
            power_present[static_cast<size_t>(owner)] = 1;
            float* pool = &power_pool[static_cast<size_t>(owner) * 2 * D];
            const float* embedded = w.embed[5].row(std::clamp(token[kColPower], 0, kVocab[5] - 1));
            const float amount = static_cast<float>(token[kColN0]);
            const float log = LogScale(amount), clip = ClipScale(amount);
            for (size_t i = 0; i < D; ++i) { pool[i] += embedded[i] * log; pool[D + i] += embedded[i] * clip; }
        }
    }

    // g = global_norm(global_numbers(numeric(scalars)) + decision + boss + misc[event] + misc[prompt] + misc[source])
    {
        float numbers[2 * kGlobalWidth];
        for (size_t i = 0; i < kGlobalWidth; ++i) {
            const bool id_like = std::find(std::begin(kIdLike), std::end(kIdLike), i) != std::end(kIdLike);
            const float v = id_like ? 0.0f : static_cast<float>(obs.global[i]);
            numbers[i] = LogScale(v);
            numbers[kGlobalWidth + i] = ClipScale(v);
        }
        float* g = x.data();
        Linear(w.global_w, w.global_b, numbers, g);
        auto add = [&](const Tensor& table, int32_t index, int32_t size) {
            const float* row = table.row(std::clamp(index, 0, size - 1));
            for (size_t i = 0; i < D; ++i) g[i] += row[i];
        };
        add(w.decision, obs.global[kGDecision], kDecisionKinds);
        add(w.boss, obs.global[kGBoss], kEncounters);
        const int32_t event = obs.global[kGEvent];
        add(w.embed[6], event > 0 ? event + kMiscEvent : event, kVocab[6]);
        add(w.embed[6], obs.global[kGChoicePrompt], kVocab[6]);
        add(w.embed[6], obs.global[kGChoiceSource], kVocab[6]);
        if (w.power_context && power_present[0]) {
            std::vector<float> added(D);
            Linear(w.power_w, w.power_b, power_pool.data(), added.data());
            for (size_t i = 0; i < D; ++i) g[i] += added[i];
        }
        if (w.features >= 2) {
            // g += global_extra(numeric([incoming, incoming over block, block over incoming]))
            float incoming = 0;
            for (size_t t = 0; t < tokens; ++t) {
                const int32_t* token = &obs.tokens[t * kTokenWidth];
                if (token[kColType] == static_cast<int32_t>(TokenType::Enemy) && token[kColN7] > 0)
                    incoming += static_cast<float>(std::max(token[kColN3], 0)) * static_cast<float>(std::max(token[kColN4], 1));
            }
            const float block = static_cast<float>(obs.global[kGBlock]);
            const float extra[3] = {incoming, std::max(incoming - block, 0.0f), std::max(block - incoming, 0.0f)};
            float scaled[6];
            for (int i = 0; i < 3; ++i) { scaled[i] = LogScale(extra[i]); scaled[3 + i] = ClipScale(extra[i]); }
            std::vector<float> added(D);
            Linear(w.extra_w, w.extra_b, scaled, added.data());
            for (size_t i = 0; i < D; ++i) g[i] += added[i];
        }
        if (w.features >= 4) {
            // model.py deck_summary: bag multiplicities, never offers or combat piles.
            float summary[kDeckNumbers]{};
            float& count = summary[kDeckNumbers - 2];
            float& upgraded = summary[kDeckNumbers - 1];
            for (size_t t = 0; t < tokens; ++t) {
                const int32_t* token = &obs.tokens[t * kTokenWidth];
                if (token[kColType] != static_cast<int32_t>(TokenType::Deck) || token[kColCard] <= 0) continue;
                const int32_t card = std::clamp(token[kColCard], 0, kVocab[1] - 1);
                const int32_t level = std::clamp(token[kColN0], 0, kCardLevels - 1);
                const float copies = static_cast<float>(std::max(token[kColN1], 0));
                const size_t at = static_cast<size_t>(card * kCardLevels + level);
                count += copies;
                if (level > 0) upgraded += copies;
                for (int32_t i = 0; i < kCardNumbers; ++i)
                    summary[i] += copies * w.card_numbers.data[at * kCardNumbers + static_cast<size_t>(i)];
                for (int32_t i = 0; i < kCardFlags; ++i)
                    summary[2 * kCardNumbers + i] += copies * w.card_flags.data[at * kCardFlags + static_cast<size_t>(i)];
            }
            if (count > 0) {
                for (int32_t i = 0; i < kCardNumbers; ++i) summary[kCardNumbers + i] = summary[i] / count;
                float scaled[2 * kDeckNumbers];
                for (int32_t i = 0; i < kDeckNumbers; ++i) {
                    scaled[i] = LogScale(summary[i]);
                    scaled[kDeckNumbers + i] = ClipScale(summary[i]);
                }
                std::vector<float> added(D);
                Linear(w.deck_w, w.deck_b, scaled, added.data());
                for (size_t i = 0; i < D; ++i) g[i] += added[i];
            }
        }
        LayerNorm(w.global_norm_w, w.global_norm_b, g, d);
    }
    // features 3: the fewest elites ahead of each travelable node, and of the best one on offer.
    std::vector<int32_t> ahead;
    int32_t best_ahead = 1 << 20;
    if (w.features >= 3) {
        bool travel = false;
        for (size_t t = 0; t < tokens && !travel; ++t)
            travel = obs.tokens[t * kTokenWidth + kColType] == static_cast<int32_t>(TokenType::MapNext);
        if (travel) {
            ahead = ElitesAhead(obs);
            for (size_t t = 0; t < tokens; ++t) {
                const int32_t* token = &obs.tokens[t * kTokenWidth];
                if (token[kColType] != static_cast<int32_t>(TokenType::MapNext)) continue;
                const size_t cell = static_cast<size_t>(std::clamp(token[kColN1], 0, kMapRows - 1)) * kMapCols +
                                    static_cast<size_t>(std::clamp(token[kColN0], 0, kMapCols - 1));
                best_ahead = std::min(best_ahead, ahead[cell]);
            }
        }
    }
    // x = token_norm(token_numbers(numeric(n0..n7)) + sum of the id embeddings)
    for (size_t t = 0; t < tokens; ++t) {
        const int32_t* token = &obs.tokens[t * kTokenWidth];
        float numbers[2 * kNumbers];
        for (size_t i = 0; i < kNumbers; ++i) {
            const float v = static_cast<float>(token[8 + i]);
            numbers[i] = LogScale(v);
            numbers[kNumbers + i] = ClipScale(v);
        }
        float* row = &x[(t + 1) * D];
        Linear(w.token_w, w.token_b, numbers, row);
        for (size_t column = 0; column < 8; ++column) {
            const int32_t table = kIdColumnTable[column];
            const float* e = w.embed[table].row(std::clamp(token[column], 0, kVocab[table] - 1));
            for (size_t i = 0; i < D; ++i) row[i] += e[i];
        }
        if (w.power_context && token[kColType] == static_cast<int32_t>(TokenType::Enemy)) {
            // Enemy n6 is the visible zero-based enemy ordinal, Power n1
            // reserves ordinal zero for the player.
            const size_t owner = static_cast<size_t>(std::clamp<int64_t>(static_cast<int64_t>(token[kColN6]) + 1,
                                                                       0, static_cast<int64_t>(tokens)));
            if (power_present[owner]) {
                std::vector<float> added(D);
                Linear(w.power_w, w.power_b, &power_pool[owner * 2 * D], added.data());
                for (size_t i = 0; i < D; ++i) row[i] += added[i];
            }
        }
        if (w.features >= 2 && token[kColCard] > 0) {
            // x += card_features(cat(numeric(card_numbers), card_flags)) + power embeddings of what the card applies
            const size_t card = static_cast<size_t>(std::min(token[kColCard], kVocab[1] - 1));
            const int32_t raw_level = token[kColType] == static_cast<int32_t>(TokenType::Shop) ? token[kColN4] : token[kColN0];
            const size_t entry = card * kCardLevels + static_cast<size_t>(std::clamp(raw_level, 0, kCardLevels - 1));
            const float* number = &w.card_numbers.data[entry * kCardNumbers];
            const float* flag = &w.card_flags.data[entry * kCardFlags];
            float printed[2 * kCardNumbers + kCardFlags];
            for (int i = 0; i < kCardNumbers; ++i) { printed[i] = LogScale(number[i]); printed[kCardNumbers + i] = ClipScale(number[i]); }
            std::copy(flag, flag + kCardFlags, printed + 2 * kCardNumbers);
            std::vector<float> added(D);
            Linear(w.card_w, w.card_b, printed, added.data());
            for (size_t i = 0; i < D; ++i) row[i] += added[i];
            for (int p = 0; p < kCardPowers; ++p) {
                const int32_t power = static_cast<int32_t>(w.card_powers.data[entry * kCardPowers + static_cast<size_t>(p)]);
                const float* e = w.embed[5].row(std::clamp(power, 0, kVocab[5] - 1));
                for (size_t i = 0; i < D; ++i) row[i] += e[i];
            }
        }
        if (!ahead.empty() && token[kColType] == static_cast<int32_t>(TokenType::MapNext)) {
            // x += route(numeric([elites ahead, elites ahead - the best option's]))
            const size_t cell = static_cast<size_t>(std::clamp(token[kColN1], 0, kMapRows - 1)) * kMapCols +
                                static_cast<size_t>(std::clamp(token[kColN0], 0, kMapCols - 1));
            const float here = static_cast<float>(ahead[cell]), more = static_cast<float>(ahead[cell] - best_ahead);
            const float scaled[4] = {LogScale(here), LogScale(more), ClipScale(here), ClipScale(more)};
            std::vector<float> added(D);
            Linear(w.route_w, w.route_b, scaled, added.data());
            for (size_t i = 0; i < D; ++i) row[i] += added[i];
        }
        LayerNorm(w.token_norm_w, w.token_norm_b, row, d);
    }

    // TransformerEncoderLayer(norm_first=True, activation=relu), no padding in a single observation.
    const int32_t heads = w.heads, head_dim = d / heads;
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    std::vector<float> normed(length * D), qkv(length * 3 * D), mixed(length * D), hidden(static_cast<size_t>(w.feedforward)), out(D), score(length);
    for (const Weights::Layer& l : w.layer) {
        normed = x;
        for (size_t s = 0; s < length; ++s) {
            LayerNorm(l.n1_w, l.n1_b, &normed[s * D], d);
            Linear(l.in_w, l.in_b, &normed[s * D], &qkv[s * 3 * D]);
        }
        for (size_t s = 0; s < length; ++s) {
            for (int32_t h = 0; h < heads; ++h) {
                const size_t offset = static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                const float* q = &qkv[s * 3 * D + offset];
                float top = -INFINITY;
                for (size_t k = 0; k < length; ++k) {
                    const float* key = &qkv[k * 3 * D + D + offset];
                    float dot = 0;
                    for (int32_t i = 0; i < head_dim; ++i) dot += q[i] * key[i];
                    score[k] = dot * scale;
                    top = std::max(top, score[k]);
                }
                float total = 0;
                for (size_t k = 0; k < length; ++k) { score[k] = std::exp(score[k] - top); total += score[k]; }
                float* m = &mixed[s * D + offset];
                std::fill(m, m + head_dim, 0.0f);
                for (size_t k = 0; k < length; ++k) {
                    const float a = score[k] / total;
                    const float* v = &qkv[k * 3 * D + 2 * D + offset];
                    for (int32_t i = 0; i < head_dim; ++i) m[i] += a * v[i];
                }
            }
        }
        for (size_t s = 0; s < length; ++s) {
            Linear(l.out_w, l.out_b, &mixed[s * D], out.data());
            float* row = &x[s * D];
            for (size_t i = 0; i < D; ++i) row[i] += out[i];
            std::copy(row, row + D, normed.begin() + static_cast<std::ptrdiff_t>(s * D));
            LayerNorm(l.n2_w, l.n2_b, &normed[s * D], d);
            Linear(l.l1_w, l.l1_b, &normed[s * D], hidden.data());
            for (float& v : hidden) v = std::max(v, 0.0f);
            Linear(l.l2_w, l.l2_b, hidden.data(), out.data());
            for (size_t i = 0; i < D; ++i) row[i] += out[i];
        }
    }
    for (size_t s = 0; s < length; ++s) LayerNorm(w.final_w, w.final_b, &x[s * D], d);
    const float* run = x.data();
    const float* h_tokens = x.data() + D;

    // logits = policy(cat(run, action_kind + token + action_target(target) + action_number(numeric(n))))
    logits.assign(actions, 0.0f);
    std::vector<float> a(2 * D), zero(D, 0.0f), projected(D), body(D);
    std::vector<float> interaction(w.action_interaction ? D : 0), added(w.action_interaction ? D : 0);
    for (size_t i = 0; i < actions; ++i) {
        const int32_t* action = &obs.actions[i * kActionWidth];
        std::copy(run, run + D, a.begin());
        float* v = &a[D];
        const float* kind = w.action_kind.row(std::clamp(action[kActKind], 0, kActionKinds - 1));
        const bool has_token = action[kActToken] >= 0 && static_cast<size_t>(action[kActToken]) < tokens;
        const bool has_target = action[kActTarget] >= 0 && static_cast<size_t>(action[kActTarget]) < tokens;
        const float* token = has_token ? h_tokens + static_cast<size_t>(action[kActToken]) * D : zero.data();
        // model.py passes a zero vector through action_target when there is no target, which adds its bias.
        const float* target = has_target ? h_tokens + static_cast<size_t>(action[kActTarget]) * D : zero.data();
        Linear(w.target_w, w.target_b, target, projected.data());
        const float n = static_cast<float>(action[kActNumber]);
        const float number[2] = {LogScale(n), ClipScale(n)};
        Linear(w.number_w, w.number_b, number, body.data());
        for (size_t k = 0; k < D; ++k) v[k] = kind[k] + token[k] + projected[k] + body[k];
        if (w.action_interaction) {
            for (size_t k = 0; k < D; ++k) interaction[k] = token[k] * target[k];
            Linear(w.interaction_w, w.interaction_zero_b, interaction.data(), added.data());
            for (size_t k = 0; k < D; ++k) v[k] += added[k];
        }
        Linear(w.p0_w, w.p0_b, a.data(), body.data());
        for (float& u : body) u = std::max(u, 0.0f);
        Linear(w.p2_w, w.p2_b, body.data(), &logits[i]);
    }
    Linear(w.v0_w, w.v0_b, run, body.data());
    for (float& u : body) u = std::max(u, 0.0f);
    Linear(w.v2_w, w.v2_b, body.data(), &value);
}

}  // namespace sts2::nosl
