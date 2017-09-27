#include "Controller.h"

#include "StreamResponse.h"

namespace Mongoose
{

	Response* Controller::serverInternalError(std::string message) {
		StreamResponse *response = new StreamResponse;

		response->setCode(HTTP_SERVER_ERROR);
		response->append("[500] Server internal error: " + message);

		return response;
	}
	Response* Controller::documentMissing(std::string message) {
		StreamResponse *response = new StreamResponse;

		response->setCode(HTTP_NOT_FOUND);
		response->append("[500] Document not found: " + message);

		return response;
	}
}
