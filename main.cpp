#include <osg/Camera>
#include <osg/Geode>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osgText/Text>
#include <osgViewer/Viewer>
#include <osgGA/GUIEventHandler>
#include <osgGA/TrackballManipulator>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <vector>

namespace
{
const float kArenaHalf = 25.0f;
const float kPlayerSpeed = 0.6f;
const float kCollectRadius = 1.7f;
const int kWinScore = 12;

struct InputState
{
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

class InputHandler : public osgGA::GUIEventHandler
{
public:
    explicit InputHandler(InputState& state) : _state(state) {}

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&) override
    {
        const bool pressed = ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN;
        const bool released = ea.getEventType() == osgGA::GUIEventAdapter::KEYUP;
        if (!pressed && !released)
            return false;

        const bool value = pressed;
        switch (ea.getKey())
        {
        case 'w':
        case 'W':
        case osgGA::GUIEventAdapter::KEY_Up:
            _state.up = value;
            return true;
        case 's':
        case 'S':
        case osgGA::GUIEventAdapter::KEY_Down:
            _state.down = value;
            return true;
        case 'a':
        case 'A':
        case osgGA::GUIEventAdapter::KEY_Left:
            _state.left = value;
            return true;
        case 'd':
        case 'D':
        case osgGA::GUIEventAdapter::KEY_Right:
            _state.right = value;
            return true;
        default:
            return false;
        }
    }

private:
    InputState& _state;
};

class GameUpdateCallback : public osg::NodeCallback
{
public:
    GameUpdateCallback(osg::MatrixTransform* player,
                       osgText::Text* hudText,
                       InputState& input,
                       std::vector<osg::ref_ptr<osg::MatrixTransform> > pickups)
        : _player(player), _hudText(hudText), _input(input), _pickups(pickups), _score(0), _elapsed(0.0)
    {
        _lastTick = std::clock();
    }

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        const std::clock_t now = std::clock();
        double dt = double(now - _lastTick) / CLOCKS_PER_SEC;
        _lastTick = now;
        if (dt <= 0.0 || dt > 0.1)
            dt = 0.016;

        _elapsed += dt;
        updatePlayer();
        updatePickups(dt);
        updateHud();

        traverse(node, nv);
    }

private:
    void updatePlayer()
    {
        osg::Vec3 pos = _player->getMatrix().getTrans();
        if (_input.up)
            pos.y() += kPlayerSpeed;
        if (_input.down)
            pos.y() -= kPlayerSpeed;
        if (_input.left)
            pos.x() -= kPlayerSpeed;
        if (_input.right)
            pos.x() += kPlayerSpeed;

        pos.x() = std::max(-kArenaHalf, std::min(kArenaHalf, pos.x()));
        pos.y() = std::max(-kArenaHalf, std::min(kArenaHalf, pos.y()));
        pos.z() = 1.0f;
        _player->setMatrix(osg::Matrix::translate(pos));
    }

    void updatePickups(double dt)
    {
        const osg::Vec3 playerPos = _player->getMatrix().getTrans();

        for (std::size_t i = 0; i < _pickups.size(); ++i)
        {
            osg::Vec3 pickupPos = _pickups[i]->getMatrix().getTrans();
            const float wobble = 0.6f * std::sin(float(_elapsed * 2.0 + i));
            pickupPos.z() = 1.0f + wobble;
            _pickups[i]->setMatrix(osg::Matrix::translate(pickupPos));

            if ((pickupPos - playerPos).length() <= kCollectRadius)
            {
                ++_score;
                resetPickup(*_pickups[i]);
            }
        }
        (void)dt;
    }

    void resetPickup(osg::MatrixTransform& pickup)
    {
        const float x = randomRange(-kArenaHalf + 2.0f, kArenaHalf - 2.0f);
        const float y = randomRange(-kArenaHalf + 2.0f, kArenaHalf - 2.0f);
        pickup.setMatrix(osg::Matrix::translate(osg::Vec3(x, y, 1.0f)));
    }

    void updateHud()
    {
        std::ostringstream ss;
        ss << "Simple 3D Collect Game\n"
           << "Move: WASD / Arrow Keys\n"
           << "Score: " << _score << " / " << kWinScore << "\n"
           << "Time: " << int(_elapsed) << " s\n";

        if (_score >= kWinScore)
            ss << "You Win! Keep collecting for a higher score.";
        else
            ss << "Collect the yellow cubes!";

        _hudText->setText(ss.str());
    }

    float randomRange(float min, float max)
    {
        const float t = float(std::rand()) / float(RAND_MAX);
        return min + (max - min) * t;
    }

    osg::observer_ptr<osg::MatrixTransform> _player;
    osg::observer_ptr<osgText::Text> _hudText;
    InputState& _input;
    std::vector<osg::ref_ptr<osg::MatrixTransform> > _pickups;
    int _score;
    double _elapsed;
    std::clock_t _lastTick;
};

osg::ref_ptr<osg::MatrixTransform> createPlayer()
{
    osg::ref_ptr<osg::Sphere> sphere = new osg::Sphere(osg::Vec3(0.0f, 0.0f, 1.0f), 1.2f);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(sphere.get());
    drawable->setColor(osg::Vec4(0.2f, 0.8f, 1.0f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform();
    mt->addChild(geode.get());
    mt->setMatrix(osg::Matrix::translate(0.0f, 0.0f, 1.0f));
    return mt;
}

osg::ref_ptr<osg::MatrixTransform> createPickup(float x, float y)
{
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 1.1f);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(1.0f, 0.9f, 0.2f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform();
    mt->addChild(geode.get());
    mt->setMatrix(osg::Matrix::translate(x, y, 1.0f));
    return mt;
}

osg::ref_ptr<osg::Node> createArena()
{
    osg::ref_ptr<osg::Box> ground = new osg::Box(osg::Vec3(0.0f, 0.0f, -0.2f), kArenaHalf * 2.2f,
                                                  kArenaHalf * 2.2f, 0.4f);
    osg::ref_ptr<osg::ShapeDrawable> groundDraw = new osg::ShapeDrawable(ground.get());
    groundDraw->setColor(osg::Vec4(0.08f, 0.12f, 0.16f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(groundDraw.get());

    osg::ref_ptr<osg::Box> wall = new osg::Box(osg::Vec3(0.0f, 0.0f, 2.0f), kArenaHalf * 2.0f, 0.5f, 4.0f);
    osg::ref_ptr<osg::ShapeDrawable> wallDraw = new osg::ShapeDrawable(wall.get());
    wallDraw->setColor(osg::Vec4(0.25f, 0.30f, 0.36f, 1.0f));

    osg::ref_ptr<osg::MatrixTransform> walls = new osg::MatrixTransform();
    for (int i = 0; i < 4; ++i)
    {
        osg::ref_ptr<osg::Geode> wallGeode = new osg::Geode();
        wallGeode->addDrawable(wallDraw.get());

        osg::ref_ptr<osg::MatrixTransform> oneWall = new osg::MatrixTransform();
        const float offset = kArenaHalf;
        switch (i)
        {
        case 0: oneWall->setMatrix(osg::Matrix::translate(0.0f, offset, 0.0f)); break;
        case 1: oneWall->setMatrix(osg::Matrix::translate(0.0f, -offset, 0.0f)); break;
        case 2: oneWall->setMatrix(osg::Matrix::rotate(osg::inDegrees(90.0f), 0.0f, 0.0f, 1.0f) *
                                   osg::Matrix::translate(offset, 0.0f, 0.0f)); break;
        case 3: oneWall->setMatrix(osg::Matrix::rotate(osg::inDegrees(90.0f), 0.0f, 0.0f, 1.0f) *
                                   osg::Matrix::translate(-offset, 0.0f, 0.0f)); break;
        }
        oneWall->addChild(wallGeode.get());
        walls->addChild(oneWall.get());
    }

    osg::ref_ptr<osg::Group> arena = new osg::Group();
    arena->addChild(geode.get());
    arena->addChild(walls.get());
    return arena;
}

osg::ref_ptr<osg::Camera> createHudCamera(osgText::Text* text)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(text);

    osg::ref_ptr<osg::Camera> camera = new osg::Camera();
    camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    camera->setClearMask(GL_DEPTH_BUFFER_BIT);
    camera->setRenderOrder(osg::Camera::POST_RENDER);
    camera->setAllowEventFocus(false);
    camera->setProjectionMatrix(osg::Matrix::ortho2D(0, 1280, 0, 720));
    camera->setViewMatrix(osg::Matrix::identity());
    camera->addChild(geode.get());
    return camera;
}
} // namespace

int main(int argc, char** argv)
{
    std::srand(static_cast<unsigned int>(std::time(0)));

    osgViewer::Viewer viewer;
    viewer.setUpViewInWindow(60, 60, 1280, 720);
    viewer.setCameraManipulator(new osgGA::TrackballManipulator());

    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->addChild(createArena().get());

    osg::ref_ptr<osg::MatrixTransform> player = createPlayer();
    root->addChild(player.get());

    std::vector<osg::ref_ptr<osg::MatrixTransform> > pickups;
    for (int i = 0; i < 8; ++i)
    {
        const float x = -kArenaHalf + 4.0f + float(std::rand() % 40);
        const float y = -kArenaHalf + 4.0f + float(std::rand() % 40);
        osg::ref_ptr<osg::MatrixTransform> pickup = createPickup(x, y);
        pickups.push_back(pickup);
        root->addChild(pickup.get());
    }

    osg::ref_ptr<osgText::Text> hudText = new osgText::Text();
    hudText->setCharacterSize(24.0f);
    hudText->setPosition(osg::Vec3(20.0f, 680.0f, 0.0f));
    hudText->setColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    hudText->setFontResolution(32, 32);

    osg::ref_ptr<osg::Camera> hudCamera = createHudCamera(hudText.get());
    root->addChild(hudCamera.get());

    InputState input;
    viewer.addEventHandler(new InputHandler(input));
    root->addUpdateCallback(new GameUpdateCallback(player.get(), hudText.get(), input, pickups));

    viewer.setSceneData(root.get());
    viewer.getCamera()->setViewMatrixAsLookAt(osg::Vec3(0.0f, -45.0f, 32.0f), osg::Vec3(0.0f, 0.0f, 0.0f),
                                              osg::Vec3(0.0f, 0.0f, 1.0f));

    return viewer.run();
}
