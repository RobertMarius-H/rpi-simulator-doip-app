#include "uds_routine_service.h"

#include "dcm_session.h"
#include "rte_update.h"

UdsRoutineService& UdsRoutineService::getInstance()
{
    static UdsRoutineService instance;
    return instance;
}

std::vector<std::uint8_t> UdsRoutineService::negative(NegativeCode code) const
{
    return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
            static_cast<std::uint8_t>(Service::ROUTINE_CONTROL),
            static_cast<std::uint8_t>(code)};
}

std::vector<std::uint8_t> UdsRoutineService::positive(Routine routine) const
{
    const auto id = static_cast<std::uint16_t>(routine);
    return {static_cast<std::uint8_t>(Service::POSITIVE_RESPONSE),
            static_cast<std::uint8_t>(Subfunction::START),
            static_cast<std::uint8_t>(id >> 8U),
            static_cast<std::uint8_t>(id)};
}

std::vector<std::uint8_t> UdsRoutineService::progress(
    ProgressState state, std::uint8_t percent) const
{
    const auto id = static_cast<std::uint16_t>(Routine::VERIFY_WITH_PROGRESS);
    return {static_cast<std::uint8_t>(Service::POSITIVE_RESPONSE),
            static_cast<std::uint8_t>(Subfunction::REQUEST_RESULTS),
            static_cast<std::uint8_t>(id >> 8U),
            static_cast<std::uint8_t>(id),
            static_cast<std::uint8_t>(state),
            percent};
}

std::vector<std::uint8_t> UdsRoutineService::handle(
    const std::uint8_t* request, std::size_t size,
    bool programmingSession, bool unlocked)
{
    if ((nullptr == request)
        || (static_cast<std::size_t>(Layout::HEADER_BYTES) > size))
    {
        return negative(NegativeCode::INVALID_LENGTH);
    }
    const std::uint8_t subfunction =
        request[static_cast<std::size_t>(Layout::SUBFUNCTION_OFFSET)];
    const std::uint16_t routineId =
        (static_cast<std::uint16_t>(request[static_cast<std::size_t>(Layout::ROUTINE_HIGH_OFFSET)])
         << 8U)
        | request[static_cast<std::size_t>(Layout::ROUTINE_LOW_OFFSET)];
    if ((static_cast<std::uint16_t>(Routine::VERIFY) != routineId)
        && (static_cast<std::uint16_t>(Routine::ACTIVATE) != routineId)
        && (static_cast<std::uint16_t>(Routine::VERIFY_WITH_PROGRESS) != routineId))
    {
        return negative(NegativeCode::OUT_OF_RANGE);
    }
    const bool progressRoutine =
        static_cast<std::uint16_t>(Routine::VERIFY_WITH_PROGRESS) == routineId;
    if ((static_cast<std::uint8_t>(Subfunction::START) != subfunction)
        && ((!progressRoutine)
            || (static_cast<std::uint8_t>(Subfunction::REQUEST_RESULTS) != subfunction)))
    {
        return negative(NegativeCode::SUBFUNCTION_NOT_SUPPORTED);
    }
    if (((static_cast<std::uint16_t>(Routine::VERIFY) == routineId)
         && (static_cast<std::size_t>(Layout::VERIFY_BYTES) != size))
        || ((static_cast<std::uint16_t>(Routine::ACTIVATE) == routineId)
            && (static_cast<std::size_t>(Layout::HEADER_BYTES) != size))
        || (progressRoutine
            && (((static_cast<std::uint8_t>(Subfunction::START) == subfunction)
                 && (static_cast<std::size_t>(Layout::VERIFY_BYTES) != size))
                || ((static_cast<std::uint8_t>(Subfunction::REQUEST_RESULTS) == subfunction)
                    && (static_cast<std::size_t>(Layout::HEADER_BYTES) != size)))))
    {
        return negative(NegativeCode::INVALID_LENGTH);
    }
    if (!programmingSession)
    {
        return negative(NegativeCode::SESSION_NOT_SUPPORTED);
    }
    if (!unlocked)
    {
        return negative(NegativeCode::SECURITY_DENIED);
    }
    auto& download = RteUpdate::getInstance();
    if (progressRoutine
        && (static_cast<std::uint8_t>(Subfunction::REQUEST_RESULTS) == subfunction))
    {
        if (RteUpdate::State::VERIFIED == download.state())
        {
            return progress(ProgressState::VERIFIED, 100);
        }
        if (RteUpdate::State::VERIFYING != download.state())
        {
            return negative(NegativeCode::SEQUENCE_ERROR);
        }
        const auto result = download.pollVerification();
        if (RteUpdate::Result::PENDING == result)
        {
            return progress(ProgressState::VERIFYING, download.verificationProgress());
        }
        return RteUpdate::Result::OK == result
            ? progress(ProgressState::VERIFIED, 100)
            : negative(NegativeCode::PROGRAMMING_FAILURE);
    }
    if ((static_cast<std::uint16_t>(Routine::VERIFY) == routineId)
        || progressRoutine)
    {
        const auto result = download.startVerification(
            request + static_cast<std::size_t>(Layout::HEADER_BYTES),
            size - static_cast<std::size_t>(Layout::HEADER_BYTES));
        if (RteUpdate::Result::INVALID_STATE == result)
        {
            return negative(NegativeCode::SEQUENCE_ERROR);
        }
        if (RteUpdate::Result::PENDING != result)
        {
            return negative(NegativeCode::PROGRAMMING_FAILURE);
        }
        pLegacyPending = !progressRoutine;
        return progressRoutine
            ? positive(Routine::VERIFY_WITH_PROGRESS) : pendingNotification();
    }
    const auto result = download.activate();
    if (RteUpdate::Result::INVALID_STATE == result)
    {
        return negative(NegativeCode::SEQUENCE_ERROR);
    }
    if (RteUpdate::Result::OK != result)
    {
        return negative(NegativeCode::PROGRAMMING_FAILURE);
    }
    DcmSession::getInstance().resetConnection();
    return positive(Routine::ACTIVATE);
}

bool UdsRoutineService::responsePending() const
{
    return pLegacyPending
        && (RteUpdate::State::VERIFYING == RteUpdate::getInstance().state());
}

std::vector<std::uint8_t> UdsRoutineService::pollResponse()
{
    const auto result = RteUpdate::getInstance().pollVerification();
    if (RteUpdate::Result::PENDING == result)
    {
        return {};
    }
    pLegacyPending = false;
    return RteUpdate::Result::OK == result
        ? positive(Routine::VERIFY) : negative(NegativeCode::PROGRAMMING_FAILURE);
}

std::vector<std::uint8_t> UdsRoutineService::pendingNotification() const
{
    return negative(NegativeCode::RESPONSE_PENDING);
}
