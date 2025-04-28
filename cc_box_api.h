/**
 * @file cc_box_api.h
 * @brief Generic congestion control box interface
 *
 * This header defines the API for a pluggable congestion control "black box"
 * which can be fed network events (ACKs, NACKs, ECN signals, timeouts) and
 * queried for its current send rate or congestion window.
 */
#ifndef CC_BOX_API_H
#define CC_BOX_API_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum cc_event_t
 * @brief Types of congestion control events
 *
 * @var CC_EVT_ACK
 * Acknowledgment received
 * @var CC_EVT_NACK
 * Fast-retransmit loss signal
 * @var CC_EVT_ECN
 * Explicit congestion notification mark received
 * @var CC_EVT_TIMEOUT
 * Retransmission timeout occurred
 */
enum cc_event_t {
    CC_EVT_ACK,
    CC_EVT_NACK,
    CC_EVT_ECN,
    CC_EVT_TIMEOUT,
};

/**
 * @typedef cc_state_t
 * @brief Opaque handle for algorithm-specific state
 */
typedef struct cc_state cc_state_t;

/**
 * @struct cc_algo_t
 * @brief Congestion control algorithm callbacks
 *
 * Each algorithm must implement these callbacks to be used within the cc_box.
 */
typedef struct cc_algo {
    /**
     * @brief Initialize algorithm state
     * @param params Algorithm-specific parameters (may be NULL)
     * @return Pointer to allocated state or NULL on failure
     */
    cc_state_t *(*init)(void *params);

    /**
     * @brief Clean up and free algorithm state
     * @param st Pointer to state returned by init()
     */
    void (*cleanup)(cc_state_t *st);

    /**
     * @brief Handle a congestion control event
     * @param st    Algorithm state
     * @param evt   Event type (ACK, NACK, ECN, TIMEOUT)
     * @param val   Event-specific value (e.g., ECN fraction)
     */
    void (*on_event)(cc_state_t *st, enum cc_event_t evt);

    /**
     * @brief Query current send rate or congestion window
     * @param st Algorithm state
     * @return Current rate (bytes/sec) or cwnd (MSS units)
     */
    uint32_t (*get_rate)(cc_state_t *st);

    /**
     * @brief Optional parameter setter for dynamic tuning
     * @param st   Algorithm state
     * @param name Name of parameter
     * @return 0 on success, negative on error
     */
    int (*set_param)(cc_state_t *st, const char *name);
} cc_algo_t;

/**
 * @struct cc_box_t
 * @brief Container tying an algorithm to its state
 */
typedef struct {
    const cc_algo_t *algo; /**< Algorithm callbacks */
    cc_state_t      *state;/**< Opaque algorithm state */
} cc_box_t;

/**
 * @brief Create a new congestion control box
 * @param algo   Algorithm callbacks
 * @param params Algorithm-specific init parameters
 * @return Pointer to cc_box_t or NULL on failure
 */
cc_box_t *cc_box_create(const cc_algo_t *algo, void *params);

/**
 * @brief Destroy a congestion control box
 * @param box Box to destroy
 */
void cc_box_destroy(cc_box_t *box);

/**
 * @brief Feed an event into the congestion control box
 * @param box Box instance
 * @param evt Event type
 */
void cc_box_event(cc_box_t *box, enum cc_event_t evt);

/**
 * @brief Query the current send rate or cwnd
 * @param box Box instance
 * @return Rate or cwnd
 */
uint32_t cc_box_get_rate(const cc_box_t *box);

#ifdef __cplusplus
}
#endif

#endif /* CC_BOX_API_H */