import os
from enum import Enum

try:
    import wxflow
except ImportError:
    raise ImportError('wxflow was not found. Cant make logger objects.')

class Logger:
    def __init__(self, name, comm=None):
        self.comm = comm
        self.logger = wxflow.Logger(name,
                                    level=os.getenv('LOG_LEVEL', 'INFO'),
                                    colored_log=False)

        self.log_levels = {'DEBUG': self.logger.debug,
                           'INFO': self.logger.info,
                           'WARNING': self.logger.warning,
                           'ERROR': self.logger.error,
                           'CRITICAL': self.logger.critical}

    def info(self, message):
        self._log(message, 'INFO')

    def debug(self, message):
        self._log(message, 'DEBUG')

    def warning(self, message):
        self._log(message, 'WARNING')

    def error(self, message):
        self._log(message, 'ERROR')

    def info_all(self, message):
        self._log_all(message, 'INFO')

    def debug_all(self, message):
        self._log_all(message, 'DEBUG')

    def warning_all(self, message):
        self._log_all(message, 'WARNING')

    def error_all(self, message):
        self._log_all(message, 'ERROR')

    def _log(self, message, level='INFO'):
        assert level.upper() in self.log_levels.keys(), f'Invalid log level: {level}'

        if self.comm.rank() == 0:
            log_method = self.log_levels.get(level.upper(), self.logger.info)
            log_method(message)

    def _log_all(self, message, level='INFO'):
        assert level.upper() in self.log_levels.keys(), f'Invalid log level: {level}'

        log_method = self.log_levels.get(level.upper(), self.logger.info)
        log_method(f'Rank {self.comm.rank()}: {message}')
