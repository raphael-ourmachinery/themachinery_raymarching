struct tm_api_registry_api *tm_global_api_registry;

static struct tm_logger_api* tm_logger_api;
static struct tm_entity_api* tm_entity_api;
static struct tm_allocator_api *tm_allocator_api;
static struct tm_the_truth_api* tm_the_truth_api;
static struct tm_the_truth_common_types_api *tm_the_truth_common_types_api;
static struct tm_localizer_api* tm_localizer_api;
static struct tm_render_graph_module_api *tm_render_graph_module_api;
static struct tm_render_graph_toolbox_api *tm_render_graph_toolbox_api;
static struct tm_properties_view_api *tm_properties_view_api;
static struct tm_render_graph_setup_api *tm_render_graph_setup_api;

#include <foundation/allocator.h>
#include <foundation/api_registry.h>
#include <foundation/api_type_hashes.h>
#include <foundation/buffer_format.h>
#include <foundation/camera.h>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/murmurhash64a.inl>
#include <foundation/the_truth.h>
#include <foundation/the_truth_types.h>
#include <foundation/unicode_symbols.h>
#include <foundation/visibility_flags.h>
#include <foundation/macros.h>

#include <plugins/creation_graph/creation_graph.h>
#include <plugins/creation_graph/creation_graph_interpreter.h>
#include <plugins/creation_graph/creation_graph_node_type.h>
#include <plugins/creation_graph/image_nodes.h>
#include <plugins/editor_views/properties.h>
#include <plugins/entity/entity.h>
#include <plugins/entity/transform_component.h>
#include <plugins/render_graph/render_graph.h>
#include <plugins/render_graph_toolbox/render_pipeline.h>
#include <plugins/render_graph_toolbox/shadow_mapping.h>
#include <plugins/render_graph_toolbox/toolbox_common.h>
#include <plugins/render_utilities/cubemap_capture_component.h>
#include <plugins/render_utilities/primitive_drawer.h>
#include <plugins/render_utilities/render_component.h>
#include <plugins/renderer/commands.h>
#include <plugins/renderer/render_backend.h>
#include <plugins/renderer/render_command_buffer.h>
#include <plugins/renderer/renderer.h>
#include <plugins/renderer/resources.h>
#include <plugins/shader_system/shader_system.h>
#include <plugins/the_machinery_shared/component_interfaces/editor_ui_interface.h>
#include <plugins/the_machinery_shared/component_interfaces/shader_interface.h>
#include <plugins/the_machinery_shared/render_context.h>
#include <plugins/ui/ui.h>
#include <plugins/ui/ui_icon.h>
#include <plugins/default_render_pipe/default_render_pipe.h>

#define TM_TT_TYPE__RAYMARCHING_COMPONENT "tm_raymarching_component"
#define TM_TT_TYPE_HASH__RAYMARCHING_COMPONENT TM_STATIC_HASH("tm_raymarching_component", 0xb481301a8c855635ULL)
enum {
    TM_TT_PROP__RAYMARCHING_COMPONENT__COLOR,
};

typedef struct tm_raymarching_component_t {
    tm_vec3_t color;
} tm_raymarching_component_t;

typedef struct tm_component_manager_o
{
    tm_entity_context_o *ctx;
    tm_allocator_i allocator;

    tm_render_graph_module_o *raymarching_module;
    tm_renderer_backend_i *rb;
    tm_the_truth_o *tt;
    bool active;
    tm_raymarching_component_t *main_component;
} tm_component_manager_o;

typedef struct tm_const_data_t {
    tm_component_manager_o *manager;
} tm_const_data_t;


static tm_ci_shader_i *shader_aspect;
static tm_ci_editor_ui_i *editor_aspect;
static tm_properties_aspect_i *properties_aspect;

static void truth__create_types(struct tm_the_truth_o* tt)
{
    tm_the_truth_property_definition_t raymarching_component_properties[] = {
        [TM_TT_PROP__RAYMARCHING_COMPONENT__COLOR] = { "color", TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__COLOR_RGB }
    };

    const tm_tt_type_t object_type = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__RAYMARCHING_COMPONENT,
            raymarching_component_properties, TM_ARRAY_COUNT(raymarching_component_properties));

    const tm_tt_id_t component = tm_the_truth_api->create_object_of_type(tt, object_type, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *component_w = tm_the_truth_api->write(tt, component);
    tm_the_truth_common_types_api->set_color_rgb(tt, component_w, TM_TT_PROP__RAYMARCHING_COMPONENT__COLOR, 
            (tm_vec3_t){ 0.1, 0.4, 0.9 }, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_api->commit(tt, component_w, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_api->set_default_object(tt, object_type, component);
    tm_the_truth_api->set_aspect(tt, object_type, TM_CI_EDITOR_UI, editor_aspect);
    tm_the_truth_api->set_aspect(tt, object_type, TM_CI_SHADER, shader_aspect);
    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__PROPERTIES, properties_aspect);
}

static bool component__load_asset(tm_component_manager_o* man, tm_entity_t e, void* c_vp, const tm_the_truth_o* tt, tm_tt_id_t asset)
{
    struct tm_raymarching_component_t* c = c_vp;
    const tm_the_truth_object_o* asset_r = tm_tt_read(tt, asset);
    c->color = tm_the_truth_common_types_api->get_color_rgb(tt, asset_r, TM_TT_PROP__RAYMARCHING_COMPONENT__COLOR);
    return true;
}


static void pass__raymarching(const void *const_data, void *runtime_data, tm_render_graph_setup_o *graph_setup)
{
    const tm_const_data_t *data = (tm_const_data_t *)const_data;
    tm_component_manager_o *manager = data->manager;
    tm_render_graph_setup_api->set_active(graph_setup, data->manager->active);
    if (!manager->active)
        return;

    tm_fullscreen_pass_setup_t setup_post = {
        .color_targets = {
            { .name = TM_DEFAULT_RENDER_PIPE_MAIN_OUTPUT_TARGET, .load_op = TM_RENDERER_RESOURCE_LOAD_OP_CLEAR }
        },
        .shader = TM_STATIC_HASH("raymarching", 0xe136d960bbe01115ULL),
    };
    
    tm_render_graph_blackboard_value v;
    v.data = &manager->main_component->color;
    tm_strhash_t key = tm_murmur_hash_string("color");
    tm_render_graph_setup_api->write_blackboard(graph_setup, key, v);

    tm_render_graph_toolbox_api->setup_fullscreen_pass(&setup_post, runtime_data, graph_setup);
}

static tm_render_graph_module_o *create_raymarching_module(tm_component_manager_o *manager)
{
    tm_render_graph_module_o *mod = tm_render_graph_module_api->create(&manager->allocator, "raymarching");

    tm_render_graph_toolbox_api->custom_fullscreen_pass(mod, pass__raymarching, &(tm_const_data_t){ .manager = manager }, sizeof(tm_const_data_t), "raymarching");


    return mod;
}

static void component__destroy(tm_component_manager_o *manager)
{
    tm_renderer_resource_command_buffer_o *res_buf;
    tm_renderer_backend_i *rb = manager->rb;

    rb->create_resource_command_buffers(rb->inst, &res_buf, 1);

    tm_render_graph_module_api->destroy(manager->raymarching_module, res_buf);

    rb->submit_resource_command_buffers(rb->inst, &res_buf, 1);
    rb->destroy_resource_command_buffers(rb->inst, &res_buf, 1);

    tm_free(&manager->allocator, manager, sizeof(*manager));
    tm_entity_api->destroy_child_allocator(manager->ctx, &manager->allocator);
}

static void component__create(struct tm_entity_context_o* ctx)
{
    uint32_t num_backends;
    tm_renderer_backend_i **backends = (tm_renderer_backend_i **)tm_global_api_registry->implementations(TM_RENDER_BACKEND_INTERFACE_NAME, &num_backends);
    if (num_backends == 0)
        return;

    tm_renderer_backend_i *backend = *backends;


    tm_allocator_i a;
    tm_entity_api->create_child_allocator(ctx, TM_TT_TYPE__RAYMARCHING_COMPONENT, &a);
    tm_component_manager_o *manager = tm_alloc(&a, sizeof(*manager));
    *manager = (tm_component_manager_o){
        .ctx = ctx,
            .rb = backend,
            .tt = tm_entity_api->the_truth(ctx),
            .allocator = a,
    };
    manager->raymarching_module = create_raymarching_module(manager);

    tm_component_i component = {
        .name = TM_TT_TYPE__RAYMARCHING_COMPONENT,
        .bytes = sizeof(struct tm_raymarching_component_t),
        .manager = (tm_component_manager_o *)manager,
        .load_asset = component__load_asset,
        .destroy = component__destroy
    };

    tm_entity_api->register_component(ctx, &component);
}

static void ci__graph_module_inject(struct tm_component_manager_o *manager, struct tm_render_graph_module_o *module)
{
    tm_render_graph_module_api->insert_extension(module, TM_DEFAULT_RENDER_PIPE_MAIN_EXTENSION_DEBUG_VISUALIZATION, manager->raymarching_module, 5.0f);
}

static void ci__init(struct tm_component_manager_o *manager, const union tm_entity_t *entities, const uint32_t *entity_indices,
        void **shader_component_data, uint32_t num_shader_datas)
{
}

static void ci__update(struct tm_component_manager_o *manager, struct tm_render_args_t *args, const union tm_entity_t *entities,
        const struct tm_transform_component_t *entity_transforms, const uint32_t *entity_indices,
        void **component_data, uint32_t num_components, const uint8_t *frustum_visibility)
{
    manager->active = num_components > 0;
    if (manager->active) {
        manager->main_component = component_data[0];
    }
}

static float properties_ui(struct tm_properties_ui_args_t *args, tm_rect_t item_rect,
        tm_tt_id_t object, uint32_t indent)
{
    const tm_the_truth_object_o *asset_obj = tm_the_truth_api->read(args->tt, object);
    const tm_tt_id_t color = tm_the_truth_api->get_subobject(args->tt, asset_obj, TM_TT_PROP__RAYMARCHING_COMPONENT__COLOR);
    item_rect.y = tm_properties_view_api->ui_color_picker(args, item_rect, "Color", NULL, color);

    return item_rect.y;
}

static const char *get_type_display_name(void)
{
    return "raymarching Component";
}

static tm_ci_shader_i *shader_aspect = &(tm_ci_shader_i){
    .init = ci__init,
    .graph_module_inject = ci__graph_module_inject,
    .update = ci__update
};

static tm_properties_aspect_i *properties_aspect = &(tm_properties_aspect_i){
    .custom_ui = properties_ui,
        .get_type_display_name = get_type_display_name,
};

static const char* component__category(void)
{
    return TM_LOCALIZE("Samples");
}

static tm_ci_editor_ui_i* editor_aspect = &(tm_ci_editor_ui_i){
    .category = component__category
};

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api* reg, bool load)
{
    tm_global_api_registry = reg;
    tm_allocator_api = reg->get(TM_ALLOCATOR_API_NAME);
    tm_entity_api = reg->get(TM_ENTITY_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_the_truth_common_types_api = reg->get(TM_THE_TRUTH_COMMON_TYPES_API_NAME);
    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    tm_render_graph_module_api = reg->get(TM_RENDER_GRAPH_MODULE_API_NAME);
    tm_render_graph_toolbox_api = reg->get(TM_RENDER_GRAPH_TOOLBOX_API_NAME);
    tm_render_graph_setup_api = reg->get(TM_RENDER_GRAPH_SETUP_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, truth__create_types);
    tm_add_or_remove_implementation(reg, load, TM_ENTITY_CREATE_COMPONENT_INTERFACE_NAME, component__create);
}
