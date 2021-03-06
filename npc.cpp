#include "npc.h"
#include "renderwindow.h"
#include <chrono>

NPC::NPC(BSplineCurve &&bsplinecurve, gsl::Vector3D color)
    : OctahedronBall{2}, curve{std::move(bsplinecurve)}, rememberedCurve{curve}
{
    mMatrix.setToIdentity();
}

void NPC::readFile(std::string filename) {
    std::ifstream inn;
    inn.open(filename.c_str());

    if (inn.is_open())
    {
        unsigned long n;
        Vertex vertex;
        inn >> n;
        mVertices.reserve(n);
        for (unsigned long i=0; i<n; i++)
        {
            inn >> vertex;
            mVertices.push_back(vertex);
        }
        inn.close();
    }
    else
    {
        qDebug() << "Error: " << filename.c_str() << " could not be opened!";
    }
}

std::optional<NPC::NPCevents> NPC::patrol(float deltaT)
{
    t += deltaT * dir;
    bool endPoint = 0.98f <= t || t < 0.f;
    if (endPoint)
        t = gsl::clamp(t, 0.f, 1.f);

    auto p = rememberedCurve(t);
    if (mRenderWindow != nullptr)
    {
        p = mRenderWindow->mapToTerrain(p);
        p.y += 1.f;
    }
    mMatrix.setPosition(p.x, p.y, p.z);

    if (endPoint)
    {
        return NPC::ENDPOINT_ARRIVED;
    }

    return std::nullopt;
}

void NPC::update()
{
    const auto deltaTime = static_cast<float>(calcDeltaTime());

    // std::cout << "current state: " << state << std::endl;

    // Handle events (if any)
    handleEvents();

    std::optional<NPCevents> event;
    switch (state)
    {
    case SLEEP:
        break;
    case PATROL:
        event = patrol(deltaTime * 0.1f);
        if (event)
            eventQueue.push(event.value());
        break;
    case LEARN:
        dir = -dir;
        if (updatePath)
        {
            rememberedCurve = curve;
            updatePathVisual();
            updatePath = false;
        }
        state = PATROL;
        // std::cout << "learned!" << std::endl;
        break;
    case CHASE:
        break;
    }
}

void NPC::handleEvents()
{
    while (!eventQueue.empty())
    {
        auto event = eventQueue.front();
        switch (event)
        {
        case ENDPOINT_ARRIVED:
            state = LEARN;
            break;
        case ITEM_TAKEN:
            updatePath = true;
            break;
        case OBSTACLE_DETECTED:
            break;
        case PLAYER_DETECTED:
            state = CHASE;
            break;
        case CHASE_ENDPOINT_ARRIVED:
            state = PATROL;
            break;
        }
        eventQueue.pop();
    }
}

void NPC::draw()
{
    if (debugLine)
    {
        glPointSize(3.f);
        glBindVertexArray(splineVAO);
        glDrawArrays(GL_LINE_STRIP, 0, splineResolution);
        glDrawArrays(GL_POINTS, splineResolution, curve.getCs().size());
    }

    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLES, 0, mVertices.size());
}

void NPC::draw(Shader *shader)
{
    if (debugLine)
    {
        gsl::Matrix4x4 mat{};
        mat.setToIdentity();
        glUniformMatrix4fv(glGetUniformLocation(shader->getProgram(), "mMatrix"), 1, GL_TRUE, mat.constData());
        glPointSize(3.f);
        glBindVertexArray(splineVAO);
        glDrawArrays(GL_LINE_STRIP, 0, splineResolution);
        glDrawArrays(GL_POINTS, splineResolution, curve.getCs().size());
    }

    glUniformMatrix4fv(glGetUniformLocation(shader->getProgram(), "mMatrix"), 1, GL_TRUE, mMatrix.constData());
    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLES, 0, mVertices.size());
}

void NPC::init()
{
    initializeOpenGLFunctions();

    // Spline curve
    glGenVertexArrays(1, &splineVAO);
    glBindVertexArray(splineVAO);

    glGenBuffers(1, &splineVBO);
    glBindBuffer(GL_ARRAY_BUFFER, splineVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), (GLvoid*)(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Send the actual vertex data
    updatePathVisual();


    // NPC
    OctahedronBall::init();

    glBindVertexArray(0);

    mInited = true;
    lastTime = std::chrono::system_clock::now();
}

void NPC::updatePathVisual()
{
    std::vector<Vertex> vertices;
    auto c = curve.getCs();
    vertices.reserve(splineResolution + c.size());

    for (int i{0}; i < splineResolution; ++i)
    {
        auto p = curve(i * 1.f / splineResolution);
        if (mapPathToTerrain && mRenderWindow != nullptr)
        {
            p = mRenderWindow->mapToTerrain(p);
            p.y += 0.1f;
        }
        vertices.emplace_back(p.x, p.y, p.z, 0.f, 1.f, 0.f);
    }

    // Control points
    for (int i{0}; i < c.size(); ++i)
    {
        auto p = c.at(i);
        vertices.emplace_back(p.x, p.y, p.z, 1.f, 0.f, 0.f);
    }

    glBindVertexArray(splineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, splineVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

NPC::~NPC()
{
    glDeleteBuffers(1, &splineVBO);
    glDeleteVertexArrays(1, &splineVAO);
}

double NPC::calcDeltaTime()
{
    std::chrono::duration<double> duration = std::chrono::system_clock::now() - lastTime;
    lastTime = std::chrono::system_clock::now();
    return duration.count();
}
