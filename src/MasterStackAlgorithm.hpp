#pragma once

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>

#include <vector>

namespace Layout::Tiled {

    struct SMasterStackNodeData {
        SMasterStackNodeData(SP<ITarget> t, bool master = false) : target(t), isMaster(master) {}

        WP<ITarget> target;
        bool        isMaster = false;
    };

    class CMasterStackAlgorithm : public ITiledAlgorithm {
      public:
        CMasterStackAlgorithm();
        virtual ~CMasterStackAlgorithm();

        virtual void                             newTarget(SP<ITarget> target);
        virtual void                             movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                             removeTarget(SP<ITarget> target);

        virtual void                             resizeTarget(const Vector2D& delta, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                             recalculate();

        virtual SP<ITarget>                      getNextCandidate(SP<ITarget> old);

        virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>          predictSizeForNewTarget();

        virtual void                             swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        void                                     cycleNext();
        void                                     cyclePrev();
        bool                                     isOnStack();
        bool                                     isFirstStack();
        bool                                     isLastStack();

      private:
        std::vector<SP<SMasterStackNodeData>> m_nodes;
        CHyprSignalListener                     m_focusCallback;
        int                                     m_focusedStackIdx = 0;
        float                                   m_mfact           = 0.5f;
        int                                     m_peekHeight      = 40;

        SP<SMasterStackNodeData>              dataFor(SP<ITarget> t);
        SP<SMasterStackNodeData>              getMasterNode();
        SP<SMasterStackNodeData>              getFocusedStackNode();
        int                                     getStackCount();
        int                                     stackIndexOf(SP<SMasterStackNodeData> node);
        void                                    setFocusedToNode(SP<SMasterStackNodeData> node);

        void                                    focusTargetUpdate(SP<ITarget> target);
        void                                    updateFocus();
    };
};
