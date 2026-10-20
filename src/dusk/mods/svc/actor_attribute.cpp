#include "actor_attribute.hpp"

#include "registry.hpp"

#include "dusk/mods/loader/loader.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace dusk::mods::svc {
namespace {

struct Resolver {
    ActorAttributeResolverHandle handle = 0;
    ActorAttributeResolveFn fn = nullptr;
    void* userData = nullptr;
};

struct PendingResolver {
    LoadedMod* owner = nullptr;
    ActorAttributeResolveFn fn = nullptr;
    void* userData = nullptr;
};

std::unordered_map<LoadedMod*, std::vector<Resolver>> s_resolvers;
ActorAttributeResolverHandle s_nextHandle = 1;

bool valid_attribute(const ActorAttribute attribute) {
    return attribute >= ACTOR_ATTRIBUTE_MOVEMENT_SPEED &&
           attribute <= ACTOR_ATTRIBUTE_STUN_DURATION;
}

ModResult register_resolver(ModContext* context, ActorAttributeResolveFn fn, void* userData,
    ActorAttributeResolverHandle* outHandle) {
    if (outHandle != nullptr) {
        *outHandle = 0;
    }

    auto* owner = mod_from_context(context);
    if (owner == nullptr || fn == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    if (s_nextHandle == 0 || s_nextHandle == std::numeric_limits<uint64_t>::max()) {
        return MOD_UNAVAILABLE;
    }

    const ActorAttributeResolverHandle handle = s_nextHandle++;
    s_resolvers[owner].push_back(Resolver{
        .handle = handle,
        .fn = fn,
        .userData = userData,
    });

    if (outHandle != nullptr) {
        *outHandle = handle;
    }
    return MOD_OK;
}

ModResult unregister_resolver(
    ModContext* context, const ActorAttributeResolverHandle handle) {
    auto* owner = mod_from_context(context);
    if (owner == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }

    const auto it = s_resolvers.find(owner);
    if (it == s_resolvers.end()) {
        return MOD_UNSUPPORTED;
    }

    auto& resolvers = it->second;
    const auto oldSize = resolvers.size();
    std::erase_if(resolvers, [handle](const Resolver& resolver) {
        return resolver.handle == handle;
    });

    const bool removed = resolvers.size() != oldSize;
    if (resolvers.empty()) {
        s_resolvers.erase(it);
    }
    return removed ? MOD_OK : MOD_UNSUPPORTED;
}

constexpr ActorAttributeService s_actorAttributeService{
    .header = SERVICE_HEADER(ActorAttributeService, ACTOR_ATTRIBUTE_SERVICE_MAJOR,
        ACTOR_ATTRIBUTE_SERVICE_MINOR),
    .register_resolver = register_resolver,
    .unregister_resolver = unregister_resolver,
    .resolve = [](const void* actor, ActorAttribute attribute, float vanillaValue) {
        return resolve_actor_attribute(static_cast<const fopAc_ac_c*>(actor),
            attribute, vanillaValue);
    },
};

}  // namespace

float resolve_actor_attribute(
    const fopAc_ac_c* actor, const ActorAttribute attribute, const float vanillaValue) {
    if (actor == nullptr || !valid_attribute(attribute) || s_resolvers.empty()) {
        return vanillaValue;
    }

    /* A callback may unregister itself, so snapshot the active chain first. */
    std::vector<PendingResolver> pending;
    for (auto& mod : ModLoader::instance().mods()) {
        if (!mod.active) {
            continue;
        }

        const auto it = s_resolvers.find(&mod);
        if (it == s_resolvers.end()) {
            continue;
        }

        for (const Resolver& resolver : it->second) {
            pending.push_back(PendingResolver{
                .owner = &mod,
                .fn = resolver.fn,
                .userData = resolver.userData,
            });
        }
    }

    ActorAttributeInfo info{
        .actor = actor,
        .attribute = attribute,
        .vanilla_value = vanillaValue,
        .current_value = vanillaValue,
    };

    for (const PendingResolver& resolver : pending) {
        if (!resolver.owner->active) {
            continue;
        }

        float candidate = info.current_value;
        try {
            if (resolver.fn(resolver.owner->context.get(), &info, &candidate,
                    resolver.userData) &&
                std::isfinite(candidate))
            {
                info.current_value = candidate;
            }
        } catch (const std::exception& exception) {
            fail_mod(*resolver.owner, MOD_ERROR,
                std::string{"Exception in actor attribute resolver: "} + exception.what());
        } catch (...) {
            fail_mod(*resolver.owner, MOD_ERROR, "Unknown exception in actor attribute resolver");
        }
    }

    return info.current_value;
}

constinit const ServiceModule g_actorAttributeModule{
    .id = ACTOR_ATTRIBUTE_SERVICE_ID,
    .majorVersion = ACTOR_ATTRIBUTE_SERVICE_MAJOR,
    .minorVersion = ACTOR_ATTRIBUTE_SERVICE_MINOR,
    .service = &s_actorAttributeService,
    .modDetached = [](LoadedMod& mod) { s_resolvers.erase(&mod); },
    .shutdown = []() {
        s_resolvers.clear();
        s_nextHandle = 1;
    },
};

}  // namespace dusk::mods::svc
