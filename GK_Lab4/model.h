#pragma once

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental\filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "shader.hpp"
#include "lights.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
using namespace std;
using namespace glm;
using namespace std::experimental::filesystem::v1;

struct Transformation {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

struct depth_info {
    GLuint FramebufferName;
    GLuint depthTexture;
    Shader depthShader;
    GLuint DepthBiasID;
    GLuint ShadowMapID;
    GLuint depthMatrixID;
    GLuint depthMapFBO;
    GLuint depthCubemap;
    Shader geom_depth_shader;
    int width;
    int height;
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

// THIS CLASS HOLDS ALL THE INFO ABOUT A MODEL TO DRAW:
// ITS MESHES, TEXTURES
// -- THIS CODE IS TAKEN FROM https://learnopengl.com/ MODEL LOADING TUTORIAL -- //
class Model 
{
public:
    /*  Model Data */
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;

    /*  Functions   */
    // constructor, expects a filepath to a 3D model.
    Model(string const &res_path, bool gamma = false) : gammaCorrection(gamma)
    {
        path p = "";
        p = absolute(p);
        p.append(res_path);
        string result = p.string();
        loadModel(result);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader shader)
    {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
    
private:
    /*  Functions   */
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('\\'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // Walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            // tangent
            if (mesh->mTangents != nullptr) {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
            }

            // bitangent
            if (mesh->mBitangents != nullptr) {
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }

            vertices.push_back(vertex);
        }
        // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures);
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if(!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        return textures;
    }
};

// USED TO DETERMINE ELEMENT
// TYPE IN THE SCENE
enum class element_type {
    SIMPLE_MODEL,
    DIRECT_LIGHT,
    POINT_LIGHT,
    SPOT_LIGHT,
    CAMERA
};

// GENERAL CLASS FOR ALL THE ELEMENTS IN THE SCENE:
// SIMPLE MODELS, LIGHTS
class scene_element {
public:
    Model model; // REPRESENTS THE MESHES USED TO DRAW THE ELEMENT
    Transformation transformation; // TRANSFORMATION OF AN ELEMENT: POSITION, ROTATION, SCALE
    string tag; // TAG USED TO IDENTIFY ELEMENTS BY NAME
    element_type type = element_type::SIMPLE_MODEL; // POOR ABSTRACTION :(   : USED TO DETERMINE TYPE OF ELEMENT
    

    scene_element(Model model, Transformation transformation, string tag) : model(model), tag(tag) {
        this->transformation = transformation;
    }
    scene_element(Model model, string tag) : model(model), tag(tag) {
        this->transformation = { glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(1,1,1) };
    }
    scene_element(Model model, Transformation transformation) : model(model)
    {
        this->transformation = transformation;
        tag = "unknown";
    }
    scene_element(Model model) : model(model) {
        this->transformation = { glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(1,1,1) };
        tag = "unknown";
    }

    // USED TO COMPUTE MODEL MATRIX FOR ELEMENT, BASED ON
    // CONTENTS OF TRANSFORMATION STRUCTURE
    glm::mat4 compute_model_from_transformation() {
        glm::mat4 model_matrix = glm::mat4(1.0f);
        model_matrix = glm::translate(model_matrix, transformation.position);
        model_matrix = glm::rotate(model_matrix, glm::radians(transformation.rotation.x), glm::vec3(1, 0, 0));
        model_matrix = glm::rotate(model_matrix, glm::radians(transformation.rotation.y), glm::vec3(0, 1, 0));
        model_matrix = glm::rotate(model_matrix, glm::radians(transformation.rotation.z), glm::vec3(0, 0, 1));
        model_matrix = glm::scale(model_matrix, transformation.scale);
        return model_matrix;
    }

    // purely virtual
    virtual void draw_element(Shader shader, glm::mat4 view, glm::mat4 projection, vector<scene_element*>& lights, depth_info depth) { }
    // basically purely virtual
    virtual void* getInfo() { return NULL; }
    // purely virtual
    virtual void use_this_light(Shader shader) { }
    // purely virtual
    virtual void update_position(bool is_rotating_light) { }
};

// DIRECTIONAL LIGHT DOESN'T FADE WITH DISTANCE,
// IT'S LIKE A SUN. IT RAYS ARE ALL PARALLEL, AND
// IT SHOULD GO IN ONE DISTANCE ONLY
class directional_light : public scene_element {
public:
    Shader default_shader; // THIS SHADER IS USED TO DRAW THE LIGHT SOURCE
    directional_light_info info; // THIS HOLDS THE POINT LIGHT INFO: POSITION, COLOR, ETC.

    directional_light(Model model, Transformation transformation, string tag, Shader shader, directional_light_info info) : scene_element(model, transformation, tag) 
    {
        default_shader = shader;
        this->info = info;
        type = element_type::DIRECT_LIGHT;
    }
    directional_light(Model model, string tag, Shader shader, directional_light_info info) : scene_element(model, tag) {
        default_shader = shader;
        this->info = info;
        type = element_type::DIRECT_LIGHT;
    }
    directional_light(Model model, Transformation transformation, Shader shader, directional_light_info info) : scene_element(model, transformation)
    {
        default_shader = shader;
        this->info = info;
        type = element_type::DIRECT_LIGHT;
    }
    directional_light(Model model, Shader shader, directional_light_info info) : scene_element(model) {
        default_shader = shader;
        this->info = info;
        type = element_type::DIRECT_LIGHT;
    }
    
    // DRAW THE LIGHT SOURCE
    void draw_element(Shader shader, glm::mat4 view, glm::mat4 projection, vector<scene_element*>& lights, depth_info depth) override {
        update_position(false);
        default_shader.use();
        default_shader.setMat4("model", compute_model_from_transformation());
        default_shader.setMat4("view", view);
        default_shader.setMat4("projection", projection);
        model.Draw(default_shader);
    }

    virtual void* getInfo() override { return &info; }

    void update_position(bool is_rotating_light) {
        transformation.position = info.position;
    }

    // SET ALL THE UNIFORMS CONNECTED TO THIS LIGHT SOURCE
    virtual void use_this_light(Shader shader) override {
        update_position(false);
        GLuint pos = glGetUniformLocation(shader.ID, "directional_light.position");
        glUniform3fv(pos, 1, &(info.position[0]));
        pos = glGetUniformLocation(shader.ID, "directional_light.light_color");
        glUniform3fv(pos, 1, &(info.light_color[0]));
        pos = glGetUniformLocation(shader.ID, "directional_light.light_power");
        glUniform1f(pos, info.light_power);
    }

    ~directional_light(){}
};

// POINT LIGHT GOES IN EVERY DIRECTION,
// FADING BASED ON DISTANCE
class point_light : public scene_element {
public:
    Shader default_shader; // THIS SHADER IS USED TO DRAW THE LIGHT SOURCE
    point_light_info info; // THIS HOLDS THE POINT LIGHT INFO: POSITION, COLOR, ETC.

    point_light(Model model, Transformation transformation, string tag, Shader shader, point_light_info info) : scene_element(model, transformation, tag)
    {
        default_shader = shader;
        this->info = info;
        type = element_type::POINT_LIGHT;
    }
    point_light(Model model, string tag, Shader shader, point_light_info info) : scene_element(model, tag) {
        default_shader = shader;
        this->info = info;
        type = element_type::POINT_LIGHT;
    }
    point_light(Model model, Transformation transformation, Shader shader, point_light_info info) : scene_element(model, transformation)
    {
        default_shader = shader;
        this->info = info;
        type = element_type::POINT_LIGHT;
    }
    point_light(Model model, Shader shader, point_light_info info) : scene_element(model) {
        default_shader = shader;
        this->info = info;
        type = element_type::POINT_LIGHT;
    }

    // DRAW THE LIGHT SOURCE
    void draw_element(Shader shader, glm::mat4 view, glm::mat4 projection, vector<scene_element*>& lights, depth_info depth) override {
        update_position(false);
        default_shader.use();
        default_shader.setMat4("model", compute_model_from_transformation());
        default_shader.setMat4("view", view);
        default_shader.setMat4("projection", projection);
        model.Draw(default_shader);
    }

    virtual void* getInfo() override { return &info; }

    // This method is supposed to update the lights model
    // position, based on the light position itself.
    // Model position is in transformation.position,
    // and light position is in info.position.
    // If the light should be rotating, we also
    // rotate the model.
    void update_position(bool is_rotating_light) {
        vec3 old_position = info.position;
        if (is_rotating_light) {
            glm::mat4 model_matrix = glm::mat4(1.0f);
            model_matrix = glm::translate(model_matrix, glm::vec3(info.position.x, info.position.y, info.position.z));
            model_matrix = glm::rotate(model_matrix, glm::radians(transformation.rotation.y), glm::vec3(0, 1, 0));
            vec4 res = model_matrix * vec4(10.2,0,0,1); // this rotates the light model around the info.position, while the light model is at offset (10.2,0,0)
            info.position.x = res.x;
            info.position.y = res.y;
            info.position.z = res.z;
        }
        transformation.position = info.position;
        info.position = old_position;
    }

    // SET ALL THE UNIFORMS CONNECTED TO THIS LIGHT SOURCE
    virtual void use_this_light(Shader shader) override {
        vec3 old_position = info.position;
        if (tag == "Rotating") {
            update_position(true);
        }
        else {
            update_position(false);
        }
        info.position = transformation.position;

        GLuint pos = glGetUniformLocation(shader.ID, "point_light.position");
        glUniform3fv(pos, 1, &(info.position[0]));
        pos = glGetUniformLocation(shader.ID, "point_light.light_color");
        glUniform3fv(pos, 1, &(info.light_color[0]));
        pos = glGetUniformLocation(shader.ID, "point_light.light_power");
        glUniform1f(pos, info.light_power);
        pos = glGetUniformLocation(shader.ID, "point_light.constant");
        glUniform1f(pos, info.constant);
        pos = glGetUniformLocation(shader.ID, "point_light.linear");
        glUniform1f(pos, info.linear);
        pos = glGetUniformLocation(shader.ID, "point_light.quadratic");
        glUniform1f(pos, info.quadratic);
        info.position = old_position;
    }

    ~point_light() {}
};

// SPOT LIGHT GOES ONLY IN FRONT OF THE LIGHT SOURCE,
// WITH LIMITED RADIUS AND IT FADES BASED ON DISTANCE
class spot_light : public scene_element {
public:
    Shader default_shader; // THIS SHADER IS USED TO DRAW THE LIGHT SOURCE
    spot_light_info* info; // THIS HOLDS THE POINT LIGHT INFO: POSITION, COLOR, ETC.

    spot_light(Model model, Transformation transformation, string tag, Shader shader, spot_light_info* info) : scene_element(model, transformation, tag)
    {
        default_shader = shader;
        this->info = info;
        type = element_type::SPOT_LIGHT;
    }
    spot_light(Model model, string tag, Shader shader, spot_light_info* info) : scene_element(model, tag) {
        default_shader = shader;
        this->info = info;
        type = element_type::SPOT_LIGHT;
    }
    spot_light(Model model, Transformation transformation, Shader shader, spot_light_info* info) : scene_element(model, transformation)
    {
        default_shader = shader;
        this->info = info;
        type = element_type::SPOT_LIGHT;
    }
    spot_light(Model model, Shader shader, spot_light_info* info) : scene_element(model) {
        default_shader = shader;
        this->info = info;
        type = element_type::SPOT_LIGHT;
    }

    // DRAW THE LIGHT SOURCE
    void draw_element(Shader shader, glm::mat4 view, glm::mat4 projection, vector<scene_element*>& lights, depth_info depth) override {
        update_position(false);
        default_shader.use();
        default_shader.setMat4("model", compute_model_from_transformation());
        default_shader.setMat4("view", view);
        default_shader.setMat4("projection", projection);
        model.Draw(default_shader);
    }

    virtual void* getInfo() override { return info; }

    void update_position(bool is_rotating_light) {
        transformation.position = info->position;
    }

    // SET ALL THE UNIFORMS CONNECTED TO LIGHT
    virtual void use_this_light(Shader shader) override {
        update_position(false);
        
        GLuint pos = glGetUniformLocation(shader.ID, "spot_light.position");
        glUniform3fv(pos, 1, &(info->position[0]));
        pos = glGetUniformLocation(shader.ID, "spot_light.light_color");
        glUniform3fv(pos, 1, &(info->light_color[0]));
        pos = glGetUniformLocation(shader.ID, "spot_light.direction");
        glUniform3fv(pos, 1, &(info->direction[0]));
        pos = glGetUniformLocation(shader.ID, "spot_light.light_power");
        glUniform1f(pos, info->light_power);
        pos = glGetUniformLocation(shader.ID, "spot_light.constant");
        glUniform1f(pos, info->constant);
        pos = glGetUniformLocation(shader.ID, "spot_light.linear");
        glUniform1f(pos, info->linear);
        pos = glGetUniformLocation(shader.ID, "spot_light.quadratic");
        glUniform1f(pos, info->quadratic);
        pos = glGetUniformLocation(shader.ID, "spot_light.cutOff");
        glUniform1f(pos, info->cutOff);
        pos = glGetUniformLocation(shader.ID, "spot_light.outerCutOff");
        glUniform1f(pos, info->outerCutOff);
    }

    ~spot_light() {}
};


// SCENE CONTAINING ALL THE SCENE ELEMENTS:
// MODELS, LIGHTS, AND ALSO SHADERS USED
// FOR PROCESSING DIRECT, POINT AND SPOT LIGHT
class scene {
public:
    vector<scene_element*> scene_collection;
    vector<scene_element*> lights;
    Shader direct;
    Shader point;
    Shader spot;

    scene(Shader direct, Shader point, Shader spot) : direct(direct), point(point), spot(spot) {}
    
    void add_scene_element(scene_element* elem) {
        scene_collection.push_back(elem);
        if (elem->type == element_type::DIRECT_LIGHT || elem->type == element_type::POINT_LIGHT || elem->type == element_type::SPOT_LIGHT) lights.push_back(elem);
    }
    // DRAW ALL THE LIGHTS AND THEN DRAW THE WHOLE SCENE FROM
    // LIGHTS PERSPECTIVE
    void draw_scene(glm::mat4 view, glm::mat4 projection, depth_info depth) {
        for (auto& light : lights) {
            light->draw_element(direct, view, projection, lights, depth);
        }
        int iter = 0;
        for (auto& light : lights) {
            iter++;
            if (light->type == element_type::DIRECT_LIGHT) directional_light_pass(direct, view, projection, depth, light, iter);
            if (light->type == element_type::POINT_LIGHT) point_light_pass(point, view, projection, depth, light, iter);
            if (light->type == element_type::SPOT_LIGHT) spot_light_pass(spot, view, projection, depth, light, iter);
        }
    }

    // EVERY LIGHT PASS IS USED TO RENDER WHOLE SCENE FROM SOME
    // LIGHTS PERSPECTIVE, AND THEN BLEND THE COLORS WITH COLORS
    // ALREADY IN THE BUFFER.
    // IN CASE OD DIRECTIONAL AND POINT LIGHT, WE FIRST PROCESS THE
    // DEPTHS OF THE SCENE TO THEN RENDER THE SCENE WITH SHADOWS

    void directional_light_pass(Shader shader, glm::mat4 view, glm::mat4 projection, depth_info depth, scene_element* light, int iter)
    {
        light->update_position(false);

        glBindFramebuffer(GL_FRAMEBUFFER, depth.FramebufferName);
        glViewport(0, 0, 1024, 1024);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // Cull back-facing triangles -> draw only front-facing triangles

        // Clear the depth buffer
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(depth.depthShader.ID);

        glm::vec3 lightInvDir = light->transformation.position;

        glm::mat4 depthProjectionMatrix = glm::ortho<float>(-200, 200, -200, 200, -50, 70);
        // The directional light is always directed to (0,0,0)
        glm::mat4 depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        // Draw everything to depth texture
        for (auto& element : scene_collection) {
            if (element->type == element_type::SIMPLE_MODEL) {
                glm::mat4 depthModelMatrix = element->compute_model_from_transformation();
                glm::mat4 depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;

                glUniformMatrix4fv(depth.depthMatrixID, 1, GL_FALSE, &depthMVP[0][0]);

                element->model.Draw(depth.depthShader);
            }
        }

        // Reset framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, depth.width, depth.height); // Render on the whole framebuffer, complete from the lower left corner to the upper right

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // Cull back-facing triangles -> draw only front-facing triangles

        // If we're processing the scene from the second, third, etc. light, we have to
        // blend the colors
        if (iter > 1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            glDepthFunc(GL_EQUAL);
        }

        shader.use();

        glm::mat4 biasMatrix(
            0.5, 0.0, 0.0, 0.0,
            0.0, 0.5, 0.0, 0.0,
            0.0, 0.0, 0.5, 0.0,
            0.5, 0.5, 0.5, 1.0
        );

        // Draw the scene
        for (auto& element : scene_collection) {
            if (element->type == element_type::SIMPLE_MODEL) {
                glm::mat4 depthModelMatrix = element->compute_model_from_transformation();
                glm::mat4 depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;

                // depthBiasMVP is used to get the texel in texture to compare depth
                glm::mat4 depthBiasMVP = biasMatrix * depthMVP;
                glUniformMatrix4fv(depth.DepthBiasID, 1, GL_FALSE, &depthBiasMVP[0][0]);

                // The first texture will always be texture with depth
                glActiveTexture(GL_TEXTURE0);
                glUniform1i(depth.ShadowMapID, 0);
                glBindTexture(GL_TEXTURE_2D, depth.depthTexture);
                light->use_this_light(shader);
                shader.setMat4("model", depthModelMatrix);
                shader.setMat4("view", view);
                shader.setMat4("projection", projection);
                element->model.Draw(shader);
            }
        }
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
    }
    
    void point_light_pass(Shader shader, glm::mat4 view, glm::mat4 projection, depth_info depth, scene_element* light, int iter) {
       
        if (light->tag == "Rotating") {
            light->update_position(true);
        }
        else {
            light->update_position(false);
        }
        
        // 0. create depth cubemap transformation matrices
        // -----------------------------------------------
        float near_plane = 1.0f;
        float far_plane = 35.0f;
        glm::vec3 lightPos = light->transformation.position;
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane); // for every direction we use perspective projection
        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

        // 1. render scene to depth cubemap
        // --------------------------------
        glViewport(0, 0, 1024, 1024);
        glBindFramebuffer(GL_FRAMEBUFFER, depth.depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        depth.geom_depth_shader.use();
        for (unsigned int i = 0; i < 6; ++i)
            depth.geom_depth_shader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
        depth.geom_depth_shader.setFloat("far_plane", far_plane);
        depth.geom_depth_shader.setVec3("lightPos", lightPos);
        // We draw whole scene to cubemap
        for (auto& element : scene_collection) {
            if (element->type == element_type::SIMPLE_MODEL) {
                glm::mat4 model = element->compute_model_from_transformation();
                depth.geom_depth_shader.setMat4("model", model);
                element->model.Draw(depth.depthShader);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. render scene as normal 
        // -------------------------
        glViewport(0, 0, depth.width, depth.height);
        shader.use();

        // If we're processing the scene from the second, third, etc. light, we have to
        // blend the colors
        if (iter > 1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            glDepthFunc(GL_EQUAL);
        }

        // Draw the scene
        for (auto& element : scene_collection) {
            if (element->type == element_type::SIMPLE_MODEL) {
                glm::mat4 model = element->compute_model_from_transformation();

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, depth.depthCubemap);
                shader.setFloat("far_plane", far_plane);
                light->use_this_light(shader);
                shader.setMat4("model", model);
                shader.setMat4("view", view);
                shader.setMat4("projection", projection);
                element->model.Draw(shader);
            }
        }

        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
    }

    void spot_light_pass(Shader shader, glm::mat4 view, glm::mat4 projection, depth_info depth, scene_element* light, int iter) {

        light->update_position(false);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, depth.width, depth.height);

        shader.use();

        if (iter > 1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            glDepthFunc(GL_EQUAL);
        }

        for (auto& element : scene_collection) {
            if (element->type == element_type::SIMPLE_MODEL) {
                glm::mat4 model = element->compute_model_from_transformation();
                light->use_this_light(shader);
                shader.setMat4("model", model);
                shader.setMat4("view", view);
                shader.setMat4("projection", projection);
                element->model.Draw(shader);
            }
        }

        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);

    }
    
    // GET A SCENE ELEMENT POINTER BY NAME
    scene_element* find_by_name(std::string name) {
        for (auto elem : scene_collection) {
            if (elem->tag == name) {
                return elem;
            }
        }
        return nullptr;
    }
};

// WE USE THIS CLASS TO SIMULATE PLAYER MOVEMENT
// IT HAS PLAYER MODEL, FPS AND TPS CAMERA, AND ALSO
// LIGHT ROTATING AROUND THE PLAYER
class player_dummy {
public:
    scene_element* player; // A PLAYER MODEL
    Camera* fps_camera; // FIRST PERSON CAMERA
    Camera* tps_camera; // THIRD PERSON CAMERA
    scene_element* connector; // CONNECTOR BETWEEN PLAYER MODEL AND ROTATING LIGHT
    scene_element* light; // POINT LIGHT, ROTATING AROUND MODEL
    point_light_info* info; // TRANSFORMATION INFO ABOUT LIGHT
    uint32 n_frames; // NUMBER OF FRAMES PASSED. IMPORTANT FOR PROCESSING ROTATION AND INPUT
    float degree = 0; // DEGREE OF ROTATING LIGHT
    uint32 stop_frame = 5; // EVERY WHICH FRAME SHOULD WE STEP ROTATING?
    player_dummy(scene_element* player, scene_element* connector, scene_element* light, Camera* fps_camera, Camera* tps_camera): player(player), fps_camera(fps_camera), tps_camera(tps_camera), connector(connector), light(light) {

        info = (point_light_info*)light->getInfo();
    }
    // THIS FUNCTION BOTH PROCESSES INPUTS (=> CHANGES CAMERA AND MODEL ROTATION
    // AND POSITION) AND IT ROTATES THE LIGHT AROUND MODEL
    // -- THIS CODE IS MOSTLY INSPIRED BY TUTORIAL ON http://www.opengl-tutorial.org/ , but with changes -- //
    void step() {
        // glfwGetTime is called only once, the first time this function is called
        static double lastTime = glfwGetTime();

        // Compute time difference between current and last frame
        double currentTime = glfwGetTime();
        float deltaTime = float(currentTime - lastTime);
        n_frames++;
        // Every stop_frame we rotate the light a bit
        if (n_frames % stop_frame == 0) { degree += 5; if (degree > 360) degree = 0; }
        connector->transformation.rotation = vec3(0, degree, 0);
        light->transformation.rotation = vec3(0, degree, 0);

        if (fps_camera->currentFrames > 0) fps_camera->currentFrames--;

        // Get mouse position
        double xpos = fps_camera->windowWidth / 2, ypos = fps_camera->windowHeight / 2;

        if (fps_camera->currentFrames == 0) {
            glfwGetCursorPos(fps_camera->window, &xpos, &ypos);
        }

        if (fps_camera->shouldChange) {
            // Reset mouse position for next frame
            glfwSetCursorPos(fps_camera->window, fps_camera->windowWidth / 2, fps_camera->windowHeight / 2);

            // Compute new orientation
            fps_camera->horizontalAngle += fps_camera->mouseSpeed * float(fps_camera->windowWidth / 2 - xpos);
            fps_camera->verticalAngle += fps_camera->mouseSpeed * float(fps_camera->windowHeight / 2 - ypos);
            if (fps_camera->verticalAngle >= 3.14f / 2) fps_camera->verticalAngle = 3.14f / 2;
            if (fps_camera->verticalAngle <= -3.14f / 2) fps_camera->verticalAngle = -3.14f / 2;
            if (fps_camera->horizontalAngle >= 3.14f * 2 || fps_camera->horizontalAngle <= -3.14f * 2) fps_camera->horizontalAngle = 0;
            glm::vec3 direction(
                cos(fps_camera->verticalAngle) * sin(fps_camera->horizontalAngle),
                sin(fps_camera->verticalAngle),
                cos(fps_camera->verticalAngle) * cos(fps_camera->horizontalAngle)
            );

            // Right vector
            glm::vec3 right = glm::vec3(
                sin(fps_camera->horizontalAngle - 3.14f / 2.0f),
                0,
                cos(fps_camera->horizontalAngle - 3.14f / 2.0f)
            );

            // Up vector
            glm::vec3 up = glm::cross(right, direction);
            // We compute only horizontal directions: players shouldn't move vertically
            glm::vec3 direction_horizontal = vec3(direction.x, 0, direction.z);
            glm::vec3 right_horizontal = vec3(right.x, 0, right.z);

            // Move forward
            if (glfwGetKey(fps_camera->window, GLFW_KEY_UP) == GLFW_PRESS) {
                fps_camera->position += direction_horizontal * deltaTime * fps_camera->speed;
                tps_camera->position += direction_horizontal * deltaTime * fps_camera->speed;
                player->transformation.position += direction_horizontal * deltaTime * fps_camera->speed;
                connector->transformation.position += direction_horizontal * deltaTime * fps_camera->speed;
                info->position += direction_horizontal * deltaTime * fps_camera->speed;
            }
            // Move backward
            if (glfwGetKey(fps_camera->window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                fps_camera->position -= direction_horizontal * deltaTime * fps_camera->speed;
                tps_camera->position -= direction_horizontal * deltaTime * fps_camera->speed;
                player->transformation.position -= direction_horizontal * deltaTime * fps_camera->speed;
                connector->transformation.position -= direction_horizontal * deltaTime * fps_camera->speed;
                info->position -= direction_horizontal * deltaTime * fps_camera->speed;
            }
            // Strafe right
            if (glfwGetKey(fps_camera->window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                fps_camera->position += right_horizontal * deltaTime * fps_camera->speed;
                tps_camera->position += right_horizontal * deltaTime * fps_camera->speed;
                player->transformation.position += right_horizontal * deltaTime * fps_camera->speed;
                connector->transformation.position += right_horizontal * deltaTime * fps_camera->speed;
                info->position += right_horizontal * deltaTime * fps_camera->speed;
            }
            // Strafe left
            if (glfwGetKey(fps_camera->window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                fps_camera->position -= right_horizontal * deltaTime * fps_camera->speed;
                tps_camera->position -= right_horizontal * deltaTime * fps_camera->speed;
                player->transformation.position -= right_horizontal * deltaTime * fps_camera->speed;
                connector->transformation.position -= right_horizontal * deltaTime * fps_camera->speed;
                info->position -= right_horizontal * deltaTime * fps_camera->speed;
            }

            float FoV = fps_camera->initialFoV;// - 5 * glfwGetMouseWheel(); // Now GLFW 3 requires setting up a callback for this. It's a bit too complicated for this beginner's tutorial, so it's disabled instead.

            // COMPUTE CAMERAS MATRICES
            if (fps_camera->justChanged) {
                fps_camera->ProjectionMatrix = fps_camera->getProjectionMatrix();
                fps_camera->ViewMatrix = fps_camera->getViewMatrix();
                tps_camera->ProjectionMatrix = fps_camera->getProjectionMatrix();
                tps_camera->ViewMatrix = fps_camera->getViewMatrix();
                fps_camera->currentFrames = fps_camera->releaseFrames;
                fps_camera->justChanged = false;
            }
            else {
                fps_camera->ProjectionMatrix = fps_camera->getProjectionMatrix();
                // FPS CAMERA VIEWMATRIX IS BASED ON FPS CAMERA POSITION AND DIRECTION
                fps_camera->ViewMatrix = glm::lookAt(
                    fps_camera->position,           // fps_camera is here
                    fps_camera->position + direction, // and looks here : at the same position, plus "direction"
                    up                  // Head is up (set to 0,-1,0 to look upside-down)
                );
                tps_camera->ProjectionMatrix = tps_camera->getProjectionMatrix();
                // TPS CAMERA VIEWMATRIX IS BASED ON FPS CAMERA POSITION, BUT FROM BEHIND, LOOKING AT THE PLAYER
                tps_camera->ViewMatrix = glm::lookAt(
                    fps_camera->position - vec3(direction.x * 20, direction.y * 20, direction.z * 20),
                    fps_camera->position,
                    up
                );
            }
            if (fps_camera->hasLight) {
                fps_camera->spot_light->direction = direction;
                fps_camera->spot_light->position = fps_camera->position;
            }
        }

        // ON Q PRESSED WE ENABLE / DISABLE CURSOR
        if (glfwGetKey(fps_camera->window, GLFW_KEY_Q) == GLFW_PRESS && fps_camera->currentFrames == 0) {
            fps_camera->currentFrames = fps_camera->releaseFrames;
            fps_camera->shouldChange = !fps_camera->shouldChange;
            if (fps_camera->shouldChange) {
                glfwSetCursorPos(fps_camera->window, fps_camera->windowWidth / 2, fps_camera->windowHeight / 2);
                glfwSetInputMode(fps_camera->window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                fps_camera->justChanged = true;
            }
            else {
                glfwSetCursorPos(fps_camera->window, fps_camera->windowWidth / 2, fps_camera->windowHeight / 2);
                glfwSetInputMode(fps_camera->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }


        // For the next frame, the "last time" will be "now"
        lastTime = currentTime;


        // WE TRANSFORM THE PLAYER MODEL AND FPS CAMERA. 
        player->transformation.rotation = glm::vec3(0, -(180 - glm::degrees(fps_camera->horizontalAngle)), 0);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 translation = glm::translate(model, glm::vec3(player->transformation.position.x, 0, player->transformation.position.z));
        glm::mat4 rotation = glm::rotate(model, glm::radians(-(180 - glm::degrees(fps_camera->horizontalAngle))), glm::vec3(0, 1, 0));
        // CAMERA SHOULD BE A BIT IN FRONT OF PLAYER MODEL
        glm::vec4 result = (translation * rotation) * glm::vec4(0,0,-1.5, 1);
        fps_camera->position.x = result.x;
        fps_camera->position.z = result.z;
    }
};


// LOAD A TEXTURE FROM FILE
unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}