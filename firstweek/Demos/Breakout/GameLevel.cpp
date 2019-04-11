#include "GameLevel.h"



GameLevel::GameLevel(Window& window, std::vector<Texture>* loadedTextures) :
	orangeBrick{ "./res/model/container/container.obj", glm::vec3{1.0f,0.0f,1.0f}, loadedTextures },
	blueBrick{ "./res/model/container/container_cardboard.obj", glm::vec3{0.2f,0.2f,8.0f}, loadedTextures },
	ironBrick{ "./res/model/container/container_hard.obj", glm::vec3{0.5f,0.5f,0.5f}, loadedTextures },
	parquet{ "./res/model/parquet/parquet.obj", loadedTextures },
	playerModel{ "./res/model/cube/cube.obj", glm::vec3{1.0f,0.0f,0.0f}, loadedTextures },
	ballModel{ "./res/model/sphere/sphere.obj", loadedTextures },
	quad{ "./res/model/quad/quad.obj", loadedTextures },
	sun{ sunPosition, sunCenter, sunAmbient0, sunDiffuse0, sunSpecular},
	pointLight{ pointLightPosition, ambient, diffuse, specular, constant, linear, quadratic },
	// first create a "frustrum", it basically specifies a matrix to project shadows
	orthoFrustrum{ sun_nearPlane, sun_farPlane, sun_left, sun_right, sun_down, sun_up },
	// now create a shadowMap (it's basically a texture containing the info about what is lit what is not)
	sunShadowMap{ orthoFrustrum, 1024.0, 1024.0 },
	// cube shadow map
	pointShadow{ 1024, 1024 },
	hdrQuad{},
	hdrFB{},
	bricksIron{50},
	bricksWood{50},
	bricksPaper{50},
	particles{1000},
	coloredQuads{1000}
{


	// generate textures for shadows
	sunShadowMap.generate();
	pointShadow.generate();

	// create shaders
	objectsShader.generate("./res/shaders/objects_wlights.shader");
	sunShadowShader.generate("./res/shaders/depth.shader");
	cubeDepthShader.generate("./res/shaders/cubeDepth.shader");
	instancesObjectsShader.generate(  "./res/shaders/instances_objects_wlights.shader");
	instancesSunShadowShader.generate("./res/shaders/instances_depth.shader");
	instancesCubeDepthShader.generate("./res/shaders/instances_cubeDepth.shader");
	instancesColoredQuadsShader.generate("./res/shaders/instances_colored_quads.shader");
	hdrShader.generate("./res/shaders/hdr.shader");


	debugDepth.generate("./res/shaders/debugDepth.shader");

	hdrFB.generate();
	hdrFB.attach2DTexture(GL_COLOR_ATTACHMENT0, window.getWidth(), window.getHeight(), 4, RGBA16, GL_FLOAT);
	hdrFB.attachRenderBuffer(GL_DEPTH_COMPONENT, window.getWidth(), window.getHeight());
	if (!hdrFB.iscomplete())
	{
		std::cerr << "Framebuffer not complete!" << std::endl;
		// TODO: add proper logging
	}

	hdrShader.bind();
	hdrShader.setUniformValue("exposure", 1.0f);
	hdrShader.unbind();
	

}

void GameLevel::load(const std::vector<std::vector<int> >& layout)
{

//	std::vector<std::vector<int> > layout = { {2,3,2,3,2,3,2,3,2,3,2,3},
//											  {3,2,3,2,3,2,3,2,3,2,3,2},
//											  {1,0,1,0,0,1,0,1,0,0,0,1} };
    // 0 : empty
	// 1 : non destroyable
	// 2+: destroyable. Each number differs in color
	// 100+: for later: special bricks

	//int numRows = layout.size();
	int maxCols = 0;
	for (size_t i = 0; i < layout.size(); i++)
	{
		for (size_t j = 0; j < layout.at(i).size(); j++)
		{
			if (layout.at(i).size() > maxCols)
			{
				maxCols = layout.at(i).size();
			}

			if (layout.at(i).at(j) != 0)
			{
				Brick brick;
				brick.transform.position = glm::vec3{i,0,j};
				brick.transform.scale = glm::vec3{1.0f};
				
				if (layout.at(i).at(j) == 1)
				{
					brick.model = &ironBrick;
					brick.destructible = false;
					bricksIron.push_back(brick);
				}
				else if (layout.at(i).at(j) == 2)
				{
					brick.model = &orangeBrick;
					brick.destructible = true;
					bricksWood.push_back(brick);
				}
				else if (layout.at(i).at(j) == 3)
				{
					brick.model = &blueBrick;
					brick.destructible = true;
					bricksPaper.push_back(brick);
				}
			}
		}
	}
	bricksPaper.setModel(&blueBrick);
	bricksWood.setModel(&orangeBrick);
	bricksIron.setModel(&ironBrick);
	particles.setModel(&ironBrick);
	coloredQuads.setModel(&quad);

	// borders of the level
	float bricksize = 1.0;
	float max_x = 0.0f + 0.5f * bricksize;
	float min_x = -(maxCols)-0.5f * bricksize + 1;
	float min_z = -(maxCols)-0.5f * bricksize + 1;
	float max_z = 0.0f + 0.5f * bricksize;

	// construct the proper orthographic projection
	projection = glm::ortho(min_x, max_x,
		min_z, max_z, 1.0f, 10.0f);
	//projection = glm::perspective(glm::radians(90.0f), 1000.0f / 1000, 0.1f, 50.0f);


	// position of the sun
	sun.eye = glm::vec3{ -(max_x + min_x) / 2.0, 6.0f, -(max_z + min_z) / 2.0 };

	// background initialization
	background.transform.scale = glm::vec3{1.2f};
	background.transform.position = glm::vec3{ maxCols / 2.0 - 0.5f * bricksize, 0.0f,maxCols / 2.0 - 0.5f * bricksize };
	background.transform.rotation = glm::vec3{ 0.0f,0.0f,0.0f };
	background.model = &parquet;

	// player initialization
	player.transform.scale = glm::vec3{0.5f,1.0f,2.0f};
	player.transform.position = glm::vec3{ 10.0f,0.0f,7.5f };
	player.transform.rotation = glm::vec3{ 0.0f,0.0f,0.0f };
	player.model = &playerModel;

	// ball initialization
	ball.radius = 0.25f;
	ball.transform.scale  = glm::vec3{1.0f};
	ball.transform.position = player.transform.position -4.0f * glm::vec3{ 1.0f,-0.0f,0.25f };
	ball.transform.rotation = glm::vec3{ 0.0f, 0.0f, 0.0f };
	ball.velocity = 2.0f * glm::vec3{ -3.0f, 0.0f, -3.0f };
	ball.model = &ballModel;

	camera = Camera{ glm::vec3{0.0f, 5.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{-1.0f, 0.0f, 0.0f} };

	// walls
	normalVectorWall1 = glm::vec3{ 0.0f, 0.0f, 1.0f };
	normalVectorWall2 = glm::vec3{ -1.0f, 0.0f, 0.0f }; 
	normalVectorWall3 = glm::vec3{ 0.0f, 0.0f, 1.0f }; 

	offsetVectorWall1 = glm::vec3{ 0.0f, 0.0f, -0.5f * bricksize };
	offsetVectorWall2 = glm::vec3{ 0.0f, 0.0f, 0.0f };
	offsetVectorWall3 = glm::vec3{ 0.0f, 0.0f, (maxCols) + 0.5f * bricksize - 1 };


	ballModel.assignShader(&objectsShader);  // new stuff with renderer
	playerModel.assignShader(&objectsShader);// new stuff with renderer
	parquet.assignShader(&objectsShader);

	{
		newBall.transform = Transform{ glm::vec3{6.0f,0.0f,5.0f}, glm::vec3{0.0f,0.0f,0.0f}, glm::vec3{1.0f} };
		newBall.model = &ballModel;
		newBall.renderer = &simple3DRenderer;
	}



}

void GameLevel::render(Window& window)
{
	Transform newBallTransf;
	newBallTransf.position = glm::vec3{ 0.8f, 0.0f, 5.0f };
	newBallTransf.scale = glm::vec3{1.0f};
	newBallTransf.rotation = glm::vec3{0.0f};

	Transform newPlayerTransf;
	newPlayerTransf.position = glm::vec3{ 1.2f, 0.4f, 5.0f };
	newPlayerTransf.scale = glm::vec3{ 1.0f };
	newPlayerTransf.rotation = glm::vec3{ 0.0f };

	

	// first submit everything
	simple3DRenderer.submit(ball.model, &newBallTransf);
	simple3DRenderer.submit(player.model, &newPlayerTransf);
	simple3DRenderer.submit(player.model, &player.transform);
	simple3DRenderer.submit(ball.model, &ball.transform);
	simple3DRenderer.submit(background.model, &background.transform);
	newBall.renderer->submit(newBall.model, &newBall.transform);


	// clear shadows
	sunShadowMap.clearShadows();
	pointShadow.clearShadows();

	// calculate sunlight's shadows
	sunShadowMap.startShadows(window, sunShadowShader, &sun);
	simple3DRenderer.draw(sunShadowShader);
	sunShadowMap.stopShadows(window, sunShadowShader);


	// calculate pointlight's shadows
	pointShadow.startShadows(window, cubeDepthShader, pointLight);
	simple3DRenderer.draw(cubeDepthShader);
	pointShadow.stopShadows(window, cubeDepthShader);

	
	sunShadowMap.startShadows(window, instancesSunShadowShader, &sun);
	bricksIron.drawInstances(instancesSunShadowShader);
	bricksWood.drawInstances(instancesSunShadowShader);
	bricksPaper.drawInstances(instancesSunShadowShader);
	particles.drawInstances(instancesSunShadowShader);
	sunShadowMap.stopShadows(window, instancesSunShadowShader);
	
	
	pointShadow.startShadows(window, instancesCubeDepthShader, pointLight);
	bricksIron.drawInstances(instancesCubeDepthShader);
	bricksWood.drawInstances(instancesCubeDepthShader);
	bricksPaper.drawInstances(instancesCubeDepthShader);
	particles.drawInstances(instancesCubeDepthShader);
	pointShadow.stopShadows(window, instancesCubeDepthShader);


	//window.clearColorBufferBit(0.5f, 0.5f, 0.5f, 1.0f);


	// render the scene
	// enable HDR framebuffer
	hdrFB.bind();
	window.clearColorBufferBit(0.5f, 0.5f, 0.5f, 1.0f);


	objectsShader.bind();
	objectsShader.setUniformMatrix("view", camera.getViewMatrix(), false);
	objectsShader.setUniformMatrix("projection", projection, false);
	objectsShader.setUniformValue("cameraPos", camera.getEye());

	// cast the light and use the shadow
	sun.cast("sun[0]", objectsShader);
	sunShadowMap.passUniforms(objectsShader, "shadowMap[0]", "lightSpaceMatrix[0]", sun.getViewMatrix());

	pointLight.cast("pointLights[0]", objectsShader);
	pointShadow.passUniforms(objectsShader, "cubeDepthMap[0]", "farPlane");

	// now the instances
	instancesObjectsShader.bind();
	instancesObjectsShader.setUniformMatrix("view", camera.getViewMatrix(), false);
	instancesObjectsShader.setUniformMatrix("projection", projection, false);
	instancesObjectsShader.setUniformValue("cameraPos", camera.getEye());

	sun.cast("sun[0]", instancesObjectsShader);
	sunShadowMap.passUniforms(instancesObjectsShader, "shadowMap[0]", "lightSpaceMatrix[0]", sun.getViewMatrix());
	pointLight.cast("pointLights[0]", instancesObjectsShader);
	pointShadow.passUniforms(instancesObjectsShader, "cubeDepthMap[0]", "farPlane");

	// draw new stuff renderer
	simple3DRenderer.draw();
	simple3DRenderer.clear();
	
	bricksIron.drawInstances( instancesObjectsShader);
	bricksWood.drawInstances( instancesObjectsShader);
	bricksPaper.drawInstances(instancesObjectsShader);
	particles.drawInstances(instancesObjectsShader);

	instancesColoredQuadsShader.bind();
	instancesColoredQuadsShader.setUniformMatrix("view", camera.getViewMatrix(), false);
	instancesColoredQuadsShader.setUniformMatrix("projection", projection, false);
	instancesColoredQuadsShader.setUniformValue("brightness", 1.0f);
	coloredQuads.drawInstances(instancesColoredQuadsShader);

	// disable HDR framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//std::cout << window.getCurrentTime() << ", " << window.getLastTime() << ", " << 1.0 / window.getLastFrameTime() << std::endl;

	// render on screen
	hdrShader.bind();
	hdrShader.setTexture(GL_TEXTURE_2D, "hdrBuffer", hdrFB.getAttachedTextureID(0));
	hdrQuad.draw();
	hdrShader.unbind();

	//sunShadowMap.drawShadowMap(window, 512, 512, debugDepth);

}

void GameLevel::update(Window& window)
{
	float dt = window.getLastFrameTime();
	float t = window.getCurrentTime();

	// manage particles
	Particle newParticle;
	newParticle.transform.position = ball.transform.position + glm::vec3{ 0.2f * (rand() / (RAND_MAX + 1.0f)) };
	newParticle.transform.scale = glm::vec3{ 0.2f * (rand() / (RAND_MAX + 1.0f)) };
	newParticle.transform.rotation = glm::vec3{ 0.0f, 360.0f * rand() / (RAND_MAX + 1.0f), 0.0f };//glm::vec3{ 360.0f * rand() / (RAND_MAX + 1.0f) };
	float random_green = 0.5 * (rand()) / (RAND_MAX + 1.0);
	newParticle.color = glm::vec4{1.0f, random_green, random_green / 5.0f, 1.0f};
	newParticle.tau = 0.25f;
	coloredQuads.push_back(newParticle);
	// update time of all particles and kill old particles

	for (size_t i = 0; i < coloredQuads.size(); i++)
	{
		Particle particle = coloredQuads.getElement(i);
		particle.tau -= dt;
		if (particle.tau <= 0.0f)
			coloredQuads.deleteElement(i);
		else
			coloredQuads.setElement(i, particle);
	}


	// update position of the ball
	ball.move(dt);

	// check for collisions
	if (ball.collisionWithRectangle(player.transform.position, player.transform.scale.x, player.transform.scale.y, player.transform.scale.z))
	{
	}
	else if (ball.collisionWithPlane(normalVectorWall1, offsetVectorWall1))
	{
	}
	else if (ball.collisionWithPlane(normalVectorWall2, offsetVectorWall2))
	{
	}
	else if (ball.collisionWithPlane(normalVectorWall3, offsetVectorWall3))
	{
	}
	else {
		bool collided = false;
		for (size_t i = 0; i < bricksWood.size(); i++)
		{
			const Brick& brick = bricksWood.getElement(i);
			if (ball.collisionWithRectangle(brick.transform.position, brick.transform.scale.x, brick.transform.scale.y, brick.transform.scale.z))
			{
				if (brick.destructible)
				{
					bricksWood.deleteElement(i);
					collided = true;
					break;
				}
			}
		}

		if (!collided)
		{
			for (size_t i = 0; i < bricksPaper.size(); i++)
			{
				const Brick& brick = bricksPaper.getElement(i);
				if (ball.collisionWithRectangle(brick.transform.position, brick.transform.scale.x, brick.transform.scale.y, brick.transform.scale.z))
				{
					if (brick.destructible)
					{
						bricksPaper.deleteElement(i);
						collided = true;
						break;
					}
				}
			}
		}

		if (!collided)
		{
			for (size_t i = 0; i < bricksIron.size(); i++)
			{
				const Brick& brick = bricksIron.getElement(i);
				if (ball.collisionWithRectangle(brick.transform.position, brick.transform.scale.x, brick.transform.scale.y, brick.transform.scale.z))
				{
					if (brick.destructible)
					{
						bricksIron.deleteElement(i);
						collided = true;
						break;
					}
				}
			}
		}

	}

	//pointLight.eye = ball.transform.position + glm::vec3{0.0f, 2.0f, 0.0f};

	pointLight.eye = glm::vec3{ 5.0 + 4.5* sin(3.0 * t), 1.5, 4.5 + 5.5*cos(3.0 * t) };
	//sun.eye = glm::vec3{ 5.0 - 10.0* sin(0.3 * t), 10.0, 5.0 - 10.0*cos(0.3 * t) };

	processCommands(window);

}

void GameLevel::processCommands(Window& window)
{
	float t = window.getCurrentTime();
	float dt = window.getLastFrameTime();

	camera.processCommands(window);
	player.processCommands(window, dt, 10.0f);

}


bool GameLevel::isCompleted()
{
	if (ball.transform.position.x >= 12.0)
	{
		return true;
	}
	return false;
}

