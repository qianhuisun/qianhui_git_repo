#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <babeltrace2/babeltrace.h>
#include <stdbool.h>

struct custom_event{
    char timestamp[32];
    char host_name[32];
    char domain[32];
    char event_name[32];
};
 
/* Sink component's private data */
struct object_send {
    /* Upstream message iterator (owned by this) */
    bt_message_iterator *message_iterator;
 
    /* Current event message index */
    uint64_t index;
};

/* Copied from write.c in sink.text.details component */
static inline
void format_uint(char *buf, uint64_t value, unsigned int base)
{
    const char *spec = "%" PRIu64;
    char *buf_start = buf;

    switch (base) {
    case 2:
    case 16:
        /* TODO: Support binary format */
        spec = "%" PRIx64;
        strcpy(buf, "0x");
        buf_start = buf + 2;
        break;
    case 8:
        spec = "%" PRIo64;
        strcpy(buf, "0");
        buf_start = buf + 1;
        break;
    case 10:
        break;
    default:
        break;
    }

    sprintf(buf_start, spec, value);
}

static inline
void format_int(char *buf, int64_t value, unsigned int base)
{
    const char *spec = "%" PRIu64;
    char *buf_start = buf;
    uint64_t abs_value = value < 0 ? (uint64_t) -value : (uint64_t) value;

    if (value < 0) {
        buf[0] = '-';
        buf_start++;
    }

    switch (base) {
    case 2:
    case 16:
        /* TODO: Support binary format */
        spec = "%" PRIx64;
        strcpy(buf_start, "0x");
        buf_start += 2;
        break;
    case 8:
        spec = "%" PRIo64;
        strcpy(buf_start, "0");
        buf_start++;
        break;
    case 10:
        break;
    default:
        break;
    }

    sprintf(buf_start, spec, abs_value);
}

/*
 * Initializes the sink component.
 */
static
bt_component_class_initialize_method_status object_send_initialize(
        bt_self_component_sink *self_component_sink,
        bt_self_component_sink_configuration *configuration,
        const bt_value *params, void *initialize_method_data)
{
    /* Allocate a private data structure */
    struct object_send *object_send = malloc(sizeof(*object_send));
 
    /* Initialize the first event message's index */
    object_send->index = 1;
 
    /* Set the component's user data to our private data structure */
    bt_self_component_set_data(
        bt_self_component_sink_as_self_component(self_component_sink),
        object_send);
 
    /*
     * Add an input port named `in` to the sink component.
     *
     * This is needed so that this sink component can be connected to a
     * filter or a source component. With a connected upstream
     * component, this sink component can create a message iterator
     * to consume messages.
     */
    bt_self_component_sink_add_input_port(self_component_sink,
        "in", NULL, NULL);
 
    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}
 
/*
 * Finalizes the sink component.
 */
static
void object_send_finalize(bt_self_component_sink *self_component_sink)
{
    /* Retrieve our private data from the component's user data */
    struct object_send *object_send = bt_self_component_get_data(
        bt_self_component_sink_as_self_component(self_component_sink));
 
    /* Free the allocated structure */
    free(object_send);
}
 
/*
 * Called when the trace processing graph containing the sink component
 * is configured.
 *
 * This is where we can create our upstream message iterator.
 */
static
bt_component_class_sink_graph_is_configured_method_status
object_send_graph_is_configured(bt_self_component_sink *self_component_sink)
{
    /* Retrieve our private data from the component's user data */
    struct object_send *object_send = bt_self_component_get_data(
        bt_self_component_sink_as_self_component(self_component_sink));
 
    /* Borrow our unique port */
    bt_self_component_port_input *in_port =
        bt_self_component_sink_borrow_input_port_by_index(
            self_component_sink, 0);
 
    /* Create the uptream message iterator */
    bt_message_iterator_create_from_sink_component(self_component_sink,
        in_port, &object_send->message_iterator);
 
    return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;
}

/* Print timestamp */
static
void send_timestamp(const bt_clock_snapshot *clock_snapshot) {
    int64_t ns_from_origin;
	char buf[32];
    bt_clock_snapshot_get_ns_from_origin_status cs_status = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, &ns_from_origin);
    if (cs_status == BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK) {
        format_int(buf, ns_from_origin, 10);
        printf("%s", buf);
    }
}

 
/*
 * Prints a line for `message`, if it's an event message, to the
 * standard send.
 */
static
void send_message(struct object_send *object_send, const bt_message *message)
{
    /* Discard if it's not an event message */
    if (bt_message_get_type(message) != BT_MESSAGE_TYPE_EVENT) {
        goto end;
    }
 
    /* Borrow the event message's event and its class */
    const bt_event *event = bt_message_event_borrow_event_const(message);
    const bt_event_class *event_class = bt_event_borrow_class_const(event);
    
    /* Get timestamp */
    const bt_clock_snapshot *clock_snapshot = bt_message_event_borrow_default_clock_snapshot_const(message);

    /* Get hostname */
	const bt_stream *stream = bt_event_borrow_stream_const(event);
	const bt_trace *trace = bt_stream_borrow_trace_const(stream);
    const bt_value *hostname_value = bt_trace_borrow_environment_entry_value_by_name_const(trace, "hostname");

    /* Get domain */
    const bt_value *domain_value = bt_trace_borrow_environment_entry_value_by_name_const(trace, "domain");

    struct custom_event custom_event_object;

    /* Get timestamp */
    int64_t ns_from_origin;
    bt_clock_snapshot_get_ns_from_origin_status cs_status = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, &ns_from_origin);
    if (cs_status == BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK) {
        format_int(custom_event_object.timestamp, ns_from_origin, 10);
    }

    /* Get host name */
    strncpy(custom_event_object.host_name, bt_value_string_get(hostname_value), sizeof(custom_event_object.host_name));

    /* Get domain */
    strncpy(custom_event_object.domain, bt_value_string_get(domain_value), sizeof(custom_event_object.domain));

    /* Get event name */
    strncpy(custom_event_object.event_name, bt_event_class_get_name(event_class), sizeof(custom_event_object.event_name));

    /* Print timestamp (ns from origin) */
    printf(", \"timestamp\":\"%s\"", custom_event_object.timestamp);

    /* Print hostname */
    printf(", \"host_name\":\"%s\"", custom_event_object.host_name);

    /* Print domain */
    printf(", \"domain\":\"%s\"", custom_event_object.domain);

    /* Print event name */
    printf(", \"event_name\":\"%s\"", custom_event_object.event_name);

    printf("\n");

    /* Increment the current event message's index */
    object_send->index++;
 
end:
    return;
}
 
/*
 * Consumes a batch of messages and writes the corresponding lines to
 * the standard send.
 */
bt_component_class_sink_consume_method_status object_send_consume(
        bt_self_component_sink *self_component_sink)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
 
    /* Retrieve our private data from the component's user data */
    struct object_send *object_send = bt_self_component_get_data(
        bt_self_component_sink_as_self_component(self_component_sink));
 
    /* Consume a batch of messages from the upstream message iterator */
    bt_message_array_const messages;
    uint64_t message_count;
    bt_message_iterator_next_status next_status =
        bt_message_iterator_next(object_send->message_iterator, &messages,
            &message_count);
 
    switch (next_status) {
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:
        /* End of iteration: put the message iterator's reference */
        bt_message_iterator_put_ref(object_send->message_iterator);
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR:
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR:
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    default:
        break;
    }
 
    /* For each consumed message */
    for (uint64_t i = 0; i < message_count; i++) {
        /* Current message */
        const bt_message *message = messages[i];
 
        /* Print line for current message if it's an event message */
        send_message(object_send, message);
 
        /* Put this message's reference */
        bt_message_put_ref(message);
    }
 
end:
    return status;
}
 
/* Mandatory */
BT_PLUGIN_MODULE();
 
/* Define the `object` plugin */
BT_PLUGIN(object);
 
/* Define the `send` sink component class */
BT_PLUGIN_SINK_COMPONENT_CLASS(send, object_send_consume);
 
/* Set some of the `send` sink component class's optional methods */
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(send,
    object_send_initialize);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(send, object_send_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(send,
    object_send_graph_is_configured);